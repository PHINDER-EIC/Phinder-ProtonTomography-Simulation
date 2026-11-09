"""Export the nanowire currents (nwCurrent ntuple) of several Geant4 runs as a dataset for the
dynamic-nanobrain network.

Each input ROOT file belongs to one class (its beam energy); several files can share the same
energy (e.g. runs with different seeds). Only events where the 8 channels of the simplified
detector (4 layers x 1 fiber x 2 ends) have signal are kept.

Usage (from ProtonTomographySimulator-build):
    python3 ../ProtonTomographySimulator/dataAnalysis/exportNNDataset.py \
        --inputs outputs/testNN/E180.root:180 outputs/testNN/E200.root:200 \
        --output outputs/testNN/dataset_180vs200.npz --t-start 4 --t-end 20

Output (.npz):
    X                float32 (n_events, 8, n_time)  nanowire current [nA], channel = layer*2 + (side > 0)
    y                int64   (n_events,)            class index (classes sorted by energy)
    energy           float32 (n_events,)            nominal beam energy of the run [MeV]
    energy_first_hit float32 (n_events,)            kinetic energy of the primary at its first hit [MeV]
    event_number     int64   (n_events,)            Geant4 event number inside its file
    file_index       int64   (n_events,)            index of the input file in --inputs
    class_energies   float32 (n_classes,)           energy of each class [MeV]
    dt_ns, t_start_ns                               sample n of X is at t = t_start_ns + n * dt_ns
                                                    (event clock: first hit of the event at tOffset)
"""
import argparse
from pathlib import Path

import numpy as np
import uproot

N_LAYERS = 4
N_CHANNELS = 2 * N_LAYERS


def parse_input(text):
    """'path/E180.root:180' -> (Path, 180.0)"""
    path, sep, energy = text.rpartition(":")
    if not sep:
        raise argparse.ArgumentTypeError(f"'{text}' must be path:energy, e.g. E180.root:180")
    return Path(path), float(energy)


def load_run(path, dt_ns, i_start, i_end):
    """Read one ROOT file and return only the events with the 8 channels.

    Returns (X, event_numbers, n_events_with_signal, charge_lost_fraction, energy_first_hit).
    """
    with uproot.open(path) as f:
        nw = f["nwCurrent"].arrays(["eventNumber", "det", "layer", "fiber", "side", "current"], library="np")
        hits = f["hits"].arrays(["eventNumber", "genTrackID", "time", "kineticEnergy"], library="np")

    if np.any(nw["det"] != 0) or np.any(nw["fiber"] != 0) or np.any(nw["layer"] >= N_LAYERS):
        raise ValueError(f"{path}: not the simplified detector (1 detector, {N_LAYERS} layers, 1 fiber per layer)")

    current = np.stack(nw["current"]).astype(np.float32)          # (rows, n_time_full)
    n_time_full = current.shape[1]
    if i_end > n_time_full:
        raise ValueError(f"{path}: --t-end is beyond the stored window ({n_time_full * dt_ns:g} ns)")

    # One row of nwCurrent per (event, fiber end) -> dense (event, channel, time)
    events, row_event = np.unique(nw["eventNumber"], return_inverse=True)
    channel = nw["layer"].astype(np.int64) * 2 + (nw["side"] > 0)
    full = np.zeros((len(events), N_CHANNELS, n_time_full), dtype=np.float32)
    present = np.zeros((len(events), N_CHANNELS), dtype=bool)
    full[row_event, channel] = current
    present[row_event, channel] = True

    # Keep complete events and crop the time window
    complete = present.all(axis=1)
    total_charge = full[complete].sum()
    X = full[complete, :, i_start:i_end]
    charge_lost = 1.0 - X.sum() / total_charge if total_charge > 0 else 0.0

    # Monte Carlo truth: kinetic energy of the primary proton at its first hit (earliest in time)
    primary = hits["genTrackID"] == 1
    order = np.lexsort((hits["time"][primary], hits["eventNumber"][primary]))   # by event, then by time
    ev_primary = hits["eventNumber"][primary][order]
    ke_primary = hits["kineticEnergy"][primary][order]
    first_events, first_index = np.unique(ev_primary, return_index=True)
    first_ke = dict(zip(first_events, ke_primary[first_index]))
    energy_first_hit = np.array([first_ke.get(e, np.nan) for e in events[complete]], dtype=np.float32)

    return X, events[complete], len(events), charge_lost, energy_first_hit


def main():
    parser = argparse.ArgumentParser(description="Export nwCurrent of Geant4 runs as a network dataset")
    parser.add_argument("--inputs", nargs="+", type=parse_input, required=True,
                        help="ROOT files as path:energy_MeV (one class per energy)")
    parser.add_argument("--output", required=True, help="output .npz file")
    parser.add_argument("--dt", type=float, default=0.005, help="Nanowire.dt of the simulation JSON [ns]")
    parser.add_argument("--t-start", type=float, default=4.0, help="start of the exported window [ns]")
    parser.add_argument("--t-end", type=float, default=20.0, help="end of the exported window [ns]")
    args = parser.parse_args()

    i_start = int(round(args.t_start / args.dt))
    i_end = int(round(args.t_end / args.dt))
    if not 0 <= i_start < i_end:
        raise ValueError("need 0 <= t-start < t-end")

    class_energies = np.array(sorted({energy for _, energy in args.inputs}), dtype=np.float32)
    X_all, y_all, energy_all, e_first_all, event_all, file_all = [], [], [], [], [], []

    for k, (path, energy) in enumerate(args.inputs):
        X, events, n_signal, charge_lost, e_first = load_run(path, args.dt, i_start, i_end)
        label = int(np.searchsorted(class_energies, energy))
        print(f"{path}: {energy:g} MeV -> class {label}: {len(X)}/{n_signal} complete events "
              f"({len(X) / n_signal:.1%}), charge outside [{args.t_start:g}, {args.t_end:g}] ns = {charge_lost:.2%}, "
              f"mean peak = {X.max(axis=2).mean():.2f} nA")
        X_all.append(X)
        y_all.append(np.full(len(X), label, dtype=np.int64))
        energy_all.append(np.full(len(X), energy, dtype=np.float32))
        e_first_all.append(e_first)
        event_all.append(events.astype(np.int64))
        file_all.append(np.full(len(X), k, dtype=np.int64))

    X = np.concatenate(X_all)
    y = np.concatenate(y_all)
    counts = np.bincount(y, minlength=len(class_energies))
    for c, (e, n) in enumerate(zip(class_energies, counts)):
        print(f"class {c} ({e:g} MeV): {n} events")
    if counts.min() < 0.8 * counts.max():
        print("WARNING: unbalanced classes; consider more events for the smaller class")

    np.savez_compressed(
        args.output,
        X=X,
        y=y,
        energy=np.concatenate(energy_all),
        energy_first_hit=np.concatenate(e_first_all),
        event_number=np.concatenate(event_all),
        file_index=np.concatenate(file_all),
        class_energies=class_energies,
        dt_ns=args.dt,
        t_start_ns=args.t_start,
    )
    print(f"Saved {args.output}: X {X.shape} ({X.nbytes / 1e6:.0f} MB uncompressed)")


if __name__ == "__main__":
    main()
