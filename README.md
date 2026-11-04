# Phinder — Scintillating-fiber proton tomography simulator

A **Geant4** Monte Carlo simulation of a proton computed tomography (pCT) system based on planes of
**scintillating fibers**. A proton beam crosses one or more *phantoms* (cylinders of tissue: bone,
lung, fat, brain…) and fiber planes placed before and after the object. The program writes a ROOT
file containing:

1. The **energy deposits** (*hits*) of every charged particle in the fiber cores.
2. The **optical photons** produced in the fibers (scintillation and Cherenkov) at the moment they are created.
3. The **optical photons leaving the fiber** through its ends (what a photodetector would see).

The whole geometry (world, beam, phantoms, detectors, layers and fibers) is defined in a **JSON**
file, so changing the configuration does not require recompiling.

---

## Contents

- [Repository layout](#repository-layout)
- [Requirements](#requirements)
- [Installation](#installation)
- [Building](#building)
- [Running](#running)
- [JSON configuration file](#json-configuration-file)
- [Geometry: how it is built and how to change it](#geometry-how-it-is-built-and-how-to-change-it)
- [The proton beam](#the-proton-beam)
- [Physics and materials](#physics-and-materials)
- [Output: the ROOT file](#output-the-root-file)
- [Analysis and plots](#analysis-and-plots)
- [Modifying the code (where everything lives)](#modifying-the-code-where-everything-lives)
- [Known limitations and issues](#known-limitations-and-issues)

---

## Repository layout

```
.
├── setup.sh                         # Environment variables (Geant4, jsoncpp, ROOT) per machine
└── ProtonTomographySimulator/
    ├── CMakeLists.txt
    ├── Generator.cc                 # main(): command-line options and Geant4 start-up
    ├── init_vis.mac, vis.mac        # Visualization macros (interactive mode)
    ├── include/, src/               # C++ code (one class per file, see table below)
    ├── data/
    │   ├── confExample.json         # Template with the JSON structure
    │   └── makeConfiguration.py     # Builds the actual configuration from the template
    ├── dataAnalysis/
    │   ├── makePlotsOneProton.C     # ROOT macro: proton energy and time per layer
    │   ├── makePlotsSeveralProtons.C# ROOT macro: photon, energy and timing plots...
    │   ├── *.py, tools/             # Legacy reconstruction scripts (see limitations)
    └── test/                        # Legacy configuration generators (old format)
```

## Requirements

| Dependency | Tested version | Used for |
|---|---|---|
| Geant4 | 11.1.2 and 11.3.2 | Simulation. Requires the data sets to be installed (including `G4NDL`, since `_HP` hadronic physics is used) and, for the graphical mode, a build with Qt/OpenGL. |
| jsoncpp | — | Reading the JSON. The **static** library `$JSONCPPDIR/lib/libjsoncpp.a` and the headers in `$JSONCPPDIR/include` are used. |
| CMake | ≥ 3.5 | Building. |
| ROOT | 6.28 / 6.36 | Only for the analysis macros (`dataAnalysis/*.C`). |
| Python 3 | — | Only to generate the JSON (`makeConfiguration.py`, no external dependencies). |

## Installation

The steps below are written for Ubuntu/Debian; on other distributions only the system package
names change. All the software is installed under `~/software` as an example; any other location
works as long as the environment variables point to it.

### 1. Clone the repository

```bash
git clone https://github.com/nbabio/Phinder-ProtonTomography-Simulation.git
cd Phinder-ProtonTomography-Simulation
```

### 2. System packages

```bash
sudo apt install build-essential cmake git python3 \
                 libexpat1-dev \
                 qtbase5-dev libqt5opengl5-dev \
                 libgl1-mesa-dev libglu1-mesa-dev libxmu-dev libxi-dev
```

The Qt and OpenGL packages are only needed for the interactive visualization.

### 3. Geant4 (from source)

```bash
mkdir -p ~/software && cd ~/software
wget https://github.com/Geant4/geant4/archive/refs/tags/v11.3.2.tar.gz -O geant4-v11.3.2.tar.gz
tar xzf geant4-v11.3.2.tar.gz                  # creates geant4-11.3.2/
mkdir geant4-v11.3.2-build && cd geant4-v11.3.2-build
cmake ../geant4-11.3.2 \
      -DCMAKE_INSTALL_PREFIX=$HOME/software/geant4-v11.3.2-install \
      -DGEANT4_INSTALL_DATA=ON \
      -DGEANT4_USE_QT=ON \
      -DGEANT4_USE_OPENGL_X11=ON
make -j$(nproc)
make install
```

`GEANT4_INSTALL_DATA=ON` downloads the physics data sets (a few GB, including `G4NDL`, needed by
the `_HP` physics). Without Qt, drop the two `GEANT4_USE_*` options; the simulator then only works
in batch mode.

### 4. jsoncpp (static library)

The `CMakeLists.txt` links `$JSONCPPDIR/lib/libjsoncpp.a`, so jsoncpp must be built as a static
library and installed into its own prefix:

```bash
cd ~/software
git clone https://github.com/open-source-parsers/jsoncpp.git jsoncpp-src
mkdir jsoncpp-build && cd jsoncpp-build
cmake ../jsoncpp-src \
      -DCMAKE_INSTALL_PREFIX=$HOME/software/jsoncpp \
      -DCMAKE_INSTALL_LIBDIR=lib \
      -DBUILD_STATIC_LIBS=ON -DBUILD_SHARED_LIBS=OFF \
      -DJSONCPP_WITH_TESTS=OFF
make -j$(nproc)
make install
```

Check that `~/software/jsoncpp/lib/libjsoncpp.a` and `~/software/jsoncpp/include/json/json.h`
exist.

### 5. ROOT (optional, for the analysis)

Only needed to run the macros in `dataAnalysis/`. Follow the official instructions at
<https://root.cern/install/>; a prebuilt binary release or `conda install -c conda-forge root`
are the quickest options. If you only want to read the output from Python,
`pip install uproot pandas` is enough.

### 6. Environment variables

These variables must be set in every new terminal before building or running. `setup.sh` sets the
paths for each of the group's machines based on `$HOSTNAME`. **On a new machine add your own
block** to `setup.sh` (or export the variables by hand):

```bash
export G4INSTALLDIR=$HOME/software/geant4-v11.3.2-install/
export JSONCPPDIR=$HOME/software/jsoncpp/          # must contain include/ and lib/libjsoncpp.a
source $G4INSTALLDIR/bin/geant4.sh                  # also sets the paths to the data sets
source /path/to/root-install/bin/thisroot.sh        # optional, for the analysis
export QT_QPA_PLATFORM=xcb                          # if the Qt visualization fails under Wayland
```

`JSONCPPDIR` is read when `cmake` runs, so it must be set **before** configuring.

## Building

From the root of the repository:

```bash
source setup.sh            # or export the variables of the previous section
mkdir -p ProtonTomographySimulator-build && cd ProtonTomographySimulator-build
cmake ../ProtonTomographySimulator
make -j$(nproc)
```

This produces the `Generator` executable and copies `init_vis.mac` and `vis.mac` to the build
directory. For an executable without a graphical interface: `cmake -DWITH_GEANT4_UIVIS=OFF ...`.

> Any change in `src/` or `include/` requires running `make` again. Changes to the JSON do **not**.

## Running

### 1. Generate the configuration

```bash
cd ProtonTomographySimulator/data
python3 makeConfiguration.py -i confExample.json -o myConf.json
```

`makeConfiguration.py` takes the template, overwrites its values and **replicates** detectors,
layers and fibers with loops. This is the recommended way to change the geometry (see below).

### 2. Batch mode (production)

```bash
cd ProtonTomographySimulator-build
./Generator --input ../ProtonTomographySimulator/data/myConf.json \
            --output output.root --number 1000 --seed 1234
```

### 3. Interactive mode (visualization)

```bash
./Generator --input myConf.json --output output.root --number 1 --geant vis
```

If `--geant` is given (with any non-empty value), the Geant4 graphical interface opens,
`init_vis.mac` is executed and **no** events are launched automatically: type, for example,
`/run/beamOn 5` in the console. Trajectories accumulate across events and are colored by charge
(`vis.mac`). With many optical photons the visualization is very slow: use few events or
`killPhotons = true`.

### Command-line options

| Option | Required | Description |
|---|---|---|
| `--input file.json` | yes | Geometry and beam configuration. |
| `--output file.root` | yes | Output file. Use the `.root` extension. It is **overwritten** if it exists. |
| `--number N` | yes | Number of events (N ≥ 1). Ignored in interactive mode. |
| `--seed S` | no (default 0) | Random generator seed. Use different seeds for jobs run in parallel. |
| `--killPhotons 0/1` | no | Overrides `Photons.killPhotons` from the JSON (see [Optical photons](#optical-photons-killphotons)). |
| `--geant X` | no | Enables interactive mode. The value is not used. |
| `--pt X` | no | Read but currently **not used**. |

The program runs on a single thread (sequential `G4RunManager`).

## JSON configuration file

**Units: lengths in cm, angles in degrees, energies in MeV, times in ns.**

```jsonc
{
  "theWorld":  { "xSizeWorld": 300, "ySizeWorld": 300, "zSizeWorld": 300 },  // FULL side length of the world
  "Photons":   { "killPhotons": false },
  "theBeam":   { ... },             // see "The proton beam"
  "Phantoms":  [ { ... }, ... ],    // list of cylinders
  "Detectors": [                    // list of detectors (air boxes)
    { ...,
      "Layers": [                   // layers inside the detector (air boxes)
        { ...,
          "Fibers": [ { ... }, ... ] // fibers inside the layer
        } ] } ]
}
```

### `Phantoms` (material cylinders)

| Key | Meaning |
|---|---|
| `name` | Name (used in the volume name `phantomPhys_<name>`). |
| `material` | Material key (see the [materials table](#available-materials)). |
| `radius`, `zsize` | Radius and full length of the cylinder (cm). Its axis is the local *z*. |
| `xPos`, `yPos`, `zPos` | Position of the center in the world (cm). |
| `xDir`, `yDir`, `zDir` | Rotations (degrees) around x, y, z. |

### `Detectors` → `Layers` → `Fibers`

The hierarchy is **world → detector → layer → fiber**, and every position/rotation is **relative
to its mother volume** (the fiber's is relative to the layer, the layer's to the detector, etc.).

| Level | Keys | Notes |
|---|---|---|
| Detector | `xPosDetector`, `yPosDetector`, `zPosDetector`, `xDirDetector`, `yDirDetector`, `zDirDetector`, `xSizeDetector`, `ySizeDetector`, `zSizeDetector` | Invisible air box. Sizes are full side lengths. |
| Layer | `xPosLayer`, …, `zDirLayer`, `xSizeLayer`, `ySizeLayer`, `zSizeLayer` | Invisible air box; must fit inside the detector. |
| Fiber | `xPosSensor`, `yPosSensor`, `zPosSensor`, `xDirSensor`, `yDirSensor`, `zDirSensor` | Position and rotation of the fiber inside the layer. |
| Fiber | `coreRadius`, `claddingRadius` | Core and cladding radii (cm). |
| Fiber | `length` | **Half-length** of the fiber (cm): the fiber is `2·length` long. |
| Fiber | `coreMaterial`, `claddingMaterial` | Material keys (e.g. `EJ204` and `scint_clad`). |
| Fiber | `xSizeSensor`, `ySizeSensor`, `zSizeSensor`, `outerRadius` | Read (or present in the template) but they do **not affect** the geometry. |

## Geometry: how it is built and how to change it

### Each fiber

Each fiber is a set of three concentric tubes whose axis is the **local z**:

| Volume | Radii | Material | Role |
|---|---|---|---|
| `FiberCorePhys_d_l_f` | 0 – `coreRadius` | `coreMaterial` | Scintillator. **The only sensitive volume** (produces the *hits*). |
| `FiberCladdingPhys_d_l_f` | `coreRadius` – `claddingRadius` | `claddingMaterial` | Lower-index cladding (total internal reflection). |
| `FiberJacketPhys_d_l_f` | `claddingRadius` – `claddingRadius + 0.01 cm` | carbon | Black coating: optical surface with zero reflectivity, absorbs every photon reaching it. |

The fiber ends are in contact with air (index 1), so photons can only escape through the **end
faces**. The jacket thickness (0.01 cm) is hard-coded in
[Fiber.cc](ProtonTomographySimulator/src/Fiber.cc); take it into account so that fibers do not
overlap each other or the layer boundaries (Geant4 checks for overlaps at construction time and
prints a warning).

### Identifiers

- `det`: index of the detector in the JSON (0, 1, …).
- `layer`: **global** layer index, **not reset** between detectors. With 2 detectors of 4 layers
  each, detector 0 holds layers 0–3 and detector 1 holds layers 4–7.
- `fiber`: index of the fiber within its layer.

The *copy number* of the core and cladding volumes encodes the three indices as
`det·100000 + layer·1000 + fiber` (e.g. `102003` → det 1, layer 2, fiber 3). The scheme therefore
supports at most **1000 fibers per layer** and **100 layers**.

### Default geometry (`makeConfiguration.py`)

- 300 × 300 × 300 cm world.
- One bone phantom: a cylinder of radius 10 cm and length 10 cm, centered at the origin, with its
  axis along z (parallel to the beam).
- Two 20 × 20 × 15 cm detectors at `z = +25 cm` (upstream of the phantom; the beam travels
  towards −z) and `z = −25 cm` (downstream).
- Each detector has 4 layers of 20 × 20 × 1 cm at local z = 5.5, 4.5, −4.5, −5.5 cm, with
  alternating z rotation 90° / 0°, so that fibers run alternately along *x* and *y* (two X-Y
  planes per detector, 9 cm apart).
- 27 fibers per layer, spaced 27/20 ≈ 0.74 cm apart, 20 cm long (`length = 10`), with an EJ-204
  core of radius 0.25 cm and PMMA cladding up to 0.35 cm. `xDirSensor = 90` rotates the fiber
  axis (local z) so it lies in the plane of the layer. All fibers are together and there are no
  gaps between them.

### Changing the geometry

The simplest way is to edit `makeConfiguration.py` and regenerate the JSON:

- **Move/add detectors**: list `zPos = [25, -25]`.
- **Number, position and orientation of layers**: lists `zLayerPos` and `zDirLayer`.
- **Number of fibers and spacing**: `NFibers` and the `xfiber` computation.
- **Fiber radii, length and materials**: `coreRadius`, `claddingRadius`, `length`,
  `coreMaterial`, `claddingMaterial`.
- **Phantoms**: the `phantom[...]` block; for several phantoms, create more dictionaries and
  append them to `thePhantoms`.
- **World size**: `theWorld[...]`. Everything must lie inside the world (the code only checks the
  lower bound of the detectors).

The generated JSON can also be edited by hand; no recompilation is needed.

## The proton beam

**Protons** are generated (hard-coded in `PrimaryGeneratorAction.cc`), starting from
`z = zBeamPosition` and travelling towards **−z**.

| Key | Meaning |
|---|---|
| `xBeamPosition`, `yBeamPosition` | Beam center in x, y (cm). |
| `xBeamSigma`, `yBeamSigma` | Gaussian width of the starting point in x, y (cm). |
| `zBeamPosition` | z plane from which protons are launched (cm). |
| `xDir`, `yDir` | Rotation (degrees) of the whole beam around x and y: simulates rotating the *gantry*. Applied to both position and direction. `zDir` is not used. |
| `energy` | Mean **kinetic** energy (MeV). |
| `energyDistribution` | `"Gaussian"` (any text containing "Gauss") → Gaussian of width `energySigma`; any other value → fixed energy. |
| `energySigma` | Energy width (MeV). |
| `tBeamSigma` | Gaussian width (centered at 0) of the emission time (ns). |
| `nParticles` | Protons per event. |
| `maxOpenAngle`, `nStep` | Angular scan (see below). |

**Beam direction (deterministic scan, not random).** The direction is `(vx, vy, −1)`
(normalized). On each event `vx` advances in steps of `2·maxOpenAngle/nStep` from
`−maxOpenAngle` to `+maxOpenAngle`; once it reaches the end it wraps around and `vy` advances. A
run of `(nStep+1)²` events therefore sweeps a grid of directions. `maxOpenAngle` is written in
degrees but is converted to radians and used as a slope (small-angle approximation). With
`maxOpenAngle = 0` the beam is parallel to the z axis.

In addition, an air cone (`gantryphysical`) is drawn at the beam position; it is **purely visual**
and does not affect the physics.

## Physics and materials

### Physics list ([ModularPhysicsList.cc](ProtonTomographySimulator/src/ModularPhysicsList.cc))

`G4EmStandardPhysics`, `G4EmExtraPhysics`, `G4DecayPhysics`, `G4HadronElasticPhysicsPHP`,
`G4HadronPhysicsFTFP_BERT_HP`, `G4StoppingPhysics`, `G4IonPhysics`, `G4NeutronTrackingCut`,
`G4RadioactiveDecayPhysics` and **`G4OpticalPhysics`** (scintillation, Cherenkov, absorption and
optical boundary processes).

### Available materials

Defined in `DetectorConstruction::ConstructMaterials()`. The JSON key must match **exactly**
(case-sensitive):

| Key | Geant4 material | Optical properties |
|---|---|---|
| `air` | `G4_AIR` | n = 1 |
| `iron`, `uranium`, `aluminium`, `carbon`, `argon`, `lead`, `silicon`, `steel` | `G4_Fe`, `G4_U`, `G4_Al`, `G4_C`, `G4_Ar`, `G4_Pb`, `G4_Si`, `G4_STAINLESS-STEEL` | — |
| `lung`, `bone`, `fat`, `brain` | `G4_LUNG_ICRP`, `G4_BONE_COMPACT_ICRU`, `G4_ADIPOSE_TISSUE_ICRP`, `G4_BRAIN_ICRP` | — |
| `EJ204` | `G4_PLASTIC_SC_VINYLTOLUENE` | EJ-204 scintillator (Eljen data): tabulated emission spectrum and absorption length (380–520 nm), n = 1.59, 10400 photons/MeV, τ = 1.8 ns, rise time 0.7 ns, Birks 0.126 mm/MeV. |
| `scint_core` | `G4_POLYSTYRENE` | Generic scintillator: n = 1.59, **1000 photons/MeV** (reduced value for testing; the real one is ~8000), τ = 3.2 ns. |
| `scint_clad` | `G4_PLEXIGLASS` (PMMA) | n = 1.49, non-scintillating. |

To add a new material, insert it into the `materials` `std::map` in that function (with its
`G4MaterialPropertiesTable` if it should take part in the optics) and recompile. **Any material
that optical photons must cross needs `RINDEX`**; otherwise the photons die at the boundary.

### Optical photons (`killPhotons`)

Scintillation produces many photons (tens of thousands per proton and fiber), which dominates the
run time. `Photons.killPhotons` (or `--killPhotons`) controls what happens to them:

- `true`: each photon created in a core is recorded in the `photons` ntuple and **killed
  immediately**. Fast. The `exitPhotons` ntuple stays **empty**.
- `false`: the photon is still recorded in `photons` and is also **propagated** along the fiber
  (reflections, absorption). Those leaving through the ends are stored in `exitPhotons`. Much
  slower, but required to study light transport and the photodetector signal.

Photons created outside the cores (e.g. Cherenkov in the cladding) are not recorded in `photons`;
with `killPhotons = true` they are killed as well.

[SteppingAction.cc](ProtonTomographySimulator/src/SteppingAction.cc) contains a line
`track->SetTrackStatus(fStopAndKill);` that, when uncommented, absorbs the photon right as it
leaves the fiber (ideal photodetector coupled to the fiber end). When commented out, the photon
keeps propagating through the air after leaving (useful for visualization).

## Output: the ROOT file

The file contains three `TTree`s. **Units: cm, MeV (particles), eV/nm (photons), ns.**
*Local* positions are given in the fiber frame (**z along the fiber**, origin at its center).
Photon directions and polarizations are also given in the local frame.

### `hits` — energy deposits in the cores

One row **per step (G4Step)** of a non-optical particle inside a core with energy deposit > 0. A
particle crossing a fiber usually produces several rows; sum them to get the total energy per
fiber.

| Branch | Type | Description |
|---|---|---|
| `eventNumber` | int | Event number. |
| `det`, `layer`, `fiber` | int | Identifiers (see [Identifiers](#identifiers)). |
| `energy` | double | Energy deposited in the step (MeV). |
| `kineticEnergy` | double | Kinetic energy of the particle at the start of the step (MeV). |
| `stepLength` | double | Step length (cm). |
| `time` | double | Global time at the start of the step (ns). |
| `localx`, `localy`, `localz` | double | Position at the start of the step in the fiber frame (cm). |
| `x`, `y`, `z` | double | Global position (cm). |
| `genID` | int | PDG code of the particle (2212 = proton, 11 = electron…). |
| `genTrackID` | int | Geant4 track ID. **1 = primary proton** (if `nParticles = 1`). |

### `photons` — optical photons at creation in a core

| Branch | Type | Description |
|---|---|---|
| `eventNumber`, `det`, `layer`, `fiber` | int | As in `hits`. |
| `process` | int | 0 = scintillation, 1 = Cherenkov. |
| `trackID`, `parentID` | int | ID of the photon and of the particle that produced it. |
| `x`, `y`, `z` / `localx`, `localy`, `localz` | double | Creation point, global / local (cm). |
| `dirx`, `diry`, `dirz` | double | Initial direction (local). |
| `polx`, `poly`, `polz` | double | Polarization (local). |
| `energy` | double | Photon energy (eV). |
| `wavelength` | double | Wavelength (nm). |
| `time` | double | Creation time (ns). |

### `exitPhotons` — photons leaving the fiber

Only filled with `killPhotons = false`. One row per photon that crosses, by (Fresnel) refraction,
the boundary between core/cladding and the outside, i.e. through the fiber **ends**.

| Branch | Type | Description |
|---|---|---|
| `eventNumber`, `det`, `layer`, `fiber` | int | Fiber the photon leaves. |
| `trackID`, `parentID`, `process` | int | As in `photons`. |
| `exitVolume` | int | 0 = leaves from the core, 1 = from the cladding. |
| `side` | int | Exit end: +1 (local z > 0) or −1 (local z < 0). |
| `x`, `y`, `z` / `localx`, `localy`, `localz` | double | Exit point on the fiber face (cm). |
| `dirInx`, `dirIny`, `dirInz` | double | Direction inside the fiber, before refraction (local). |
| `dirOutx`, `dirOuty`, `dirOutz` | double | Direction after leaving (local). |
| `polx`, `poly`, `polz` | double | Polarization (local). |
| `energy`, `wavelength` | double | eV, nm. |
| `time` | double | Global arrival time at the fiber end (ns). |
| `trackLength` | double | Path length travelled since creation (cm). |

Photons in `photons` and `exitPhotons` can be matched through `(eventNumber, trackID)`.

### Quick reading

```python
import ROOT
f = ROOT.TFile("output.root")
f.hits.Draw("energy", "genTrackID==1")              # deposits of the primary proton
f.exitPhotons.Draw("time", "side==1")               # arrival times at one fiber end
```

```python
import uproot  # alternative without ROOT
df = uproot.open("output.root")["hits"].arrays(library="pd")
```

## Analysis and plots

The up-to-date macros live in `ProtonTomographySimulator/dataAnalysis/`:

```bash
# Kinetic energy and time of the primary proton's first hit in each layer -> firstHit.png
root -l -b -q 'makePlotsOneProton.C("output.root")'

# Full set of plots in the given directory (png + pdf)
root -l -b -q 'makePlotsSeveralProtons.C("output.root", "plots")'
```

`makePlotsSeveralProtons.C` produces: photon creation position and time, photons and deposited
energy per proton, position and time of signal photons, spectra, angular acceptance, trapping
efficiency versus exit angle, and propagation time versus optical path (the last ones use
`exitPhotons`, so they require `killPhotons = false`). The optional third argument is the number
of events used for normalization.

## Modifying the code (where everything lives)

| I want to change… | File |
|---|---|
| Command-line options, main flow | [Generator.cc](ProtonTomographySimulator/Generator.cc) |
| JSON reading (adding new keys) | [ConfigurationGeometry.cc](ProtonTomographySimulator/src/ConfigurationGeometry.cc) |
| Materials and optical properties | [DetectorConstruction.cc](ProtonTomographySimulator/src/DetectorConstruction.cc) |
| Fiber shape, jacket, optical surface | [Fiber.cc](ProtonTomographySimulator/src/Fiber.cc) |
| Layers / detectors / phantoms | `Layer.cc`, `Detector.cc`, `Phantom.cc` |
| Rotations and coordinate transformations | `GeomObject.cc` (base class of all volumes) |
| Particle type, primary generation | [PrimaryGeneratorAction.cc](ProtonTomographySimulator/src/PrimaryGeneratorAction.cc) |
| Beam distribution (position, energy, angular scan) | [Beam.cc](ProtonTomographySimulator/src/Beam.cc) |
| Physics list | [ModularPhysicsList.cc](ProtonTomographySimulator/src/ModularPhysicsList.cc) |
| Information stored for each hit | [FiberSensor.cc](ProtonTomographySimulator/src/FiberSensor.cc), `FiberSensorHit.hh`, [EventAction.cc](ProtonTomographySimulator/src/EventAction.cc) |
| Ntuple branch definitions | [RunAction.cc](ProtonTomographySimulator/src/RunAction.cc) |
| Recording / killing photons at creation | [StackingAction.cc](ProtonTomographySimulator/src/StackingAction.cc) |
| Recording photons leaving the fiber | [SteppingAction.cc](ProtonTomographySimulator/src/SteppingAction.cc) |
| Visualization | `vis.mac` (copied to the build directory when `cmake` runs) |

To **add a branch** to an ntuple: create the column in `RunAction::BeginOfRunAction` and fill it
with the **same column index** in `EventAction` (ntuple 0), `StackingAction` (ntuple 1) or
`SteppingAction` (ntuple 2). The creation order determines the index.

To **add a JSON parameter**: read it in the `ConfigurationGeometry` constructor (following the
pattern `atof(root[...][...].asString().c_str()) * unit`), store it as a member, add a *getter*
and use it where needed. A missing key is read as 0.

## Known limitations and issues

- **`--number` is mandatory**: if it is not given, the number of events is left uninitialized.
- **Poisson distribution of the number of particles**: the code looks for the key
  `particleDistribution`, but the JSON uses `nParticlesDistribution`, so a fixed number is always
  used. Moreover, the Poisson generator is not initialized: enabling it without fixing `Beam.cc`
  would cause a crash.
- `data/confExample.json` is only a structural template: it uses the material `"Lead"`, which does
  not exist (the correct key is `lead`). Always use the JSON produced by `makeConfiguration.py`.
- Optical properties (refractive indices, spectra, light yield) are **hard-coded**, not set in the
  JSON.
- The Python scripts in `dataAnalysis/` (`analysis.py`, `makeHLTuple.py`, `trackReconstruction*.py`,
  `makePOCA.py`, `BackFilteredPropagation.py`, `basicPlots.py` and `tools/`) and those in `test/`
  come from an earlier version of the simulator based on LGAD sensors (branches `lgad`, `xpad`,
  `toa`, `tot`, key `Sensors`…). **They are not compatible with the current output** and are kept
  as a reference for future track reconstruction.
- The `hits` ntuple stores individual steps, not hits aggregated per fiber, and includes no
  digitization (resolution, threshold) or photodetector simulation.
- Single-threaded execution.
