#include "NanowireReadout.hh"

#include "G4AnalysisManager.hh"
#include "G4Exception.hh"

#include <algorithm>
#include <cmath>

//----------------------------------------------------------------------//
// Constructor: precomputes the bin-averaged single-photon kernel       //
// (same as bin_averaged_pulse in nanowire_detector_stage7.py)          //
//----------------------------------------------------------------------//
NanowireReadout::NanowireReadout(double dt_ns, double tmax_ns, double tauRise_ns, double tauDecay_ns,
                                 double responsivity_nA, double tOffset_ns, int minPhotons_)
    : dt(dt_ns), tmax(tmax_ns), tauR(tauRise_ns), tauD(tauDecay_ns),
      R(responsivity_nA), tOffset(tOffset_ns), minPhotons(minPhotons_) {

    if(dt <= 0 || tmax <= 0)
        G4Exception("NanowireReadout", "NW001", FatalException, "Nanowire dt and tmax must be positive");
    if(tauR <= 0 || tauR >= tauD)
        G4Exception("NanowireReadout", "NW002", FatalException, "Nanowire tauRise must be positive and smaller than tauDecay");

    nTime = (int)std::lround(tmax / dt);
    if(nTime < 1 || std::abs(nTime * dt - tmax) > 1e-9)
        G4Exception("NanowireReadout", "NW003", FatalException, "Nanowire tmax must be an integer multiple of dt");

    double tPeak = tauR * tauD / (tauD - tauR) * std::log(tauD / tauR);
    peak = std::exp(-tPeak / tauD) - std::exp(-tPeak / tauR);

    kernel.resize(nTime);
    for(int n = 0; n < nTime; n++)
        kernel[n] = R * (PulseIntegral(n * dt) - PulseIntegral(n * dt - dt)) / dt;

    current.assign(nTime, 0.f);   // Size fixed here: the ntuple column is bound to this vector
}

//----------------------------------------------------------------------//
// Integral of the unit-peak bi-exponential pulse from 0 to t [ns]      //
//----------------------------------------------------------------------//
double NanowireReadout::PulseIntegral(double t) const {
    if(t <= 0) return 0.;
    return (tauD * (1 - std::exp(-t / tauD)) - tauR * (1 - std::exp(-t / tauR))) / peak;
}

//----------------------------------------------------------------------//
// Clears the photons of the previous event                             //
//----------------------------------------------------------------------//
void NanowireReadout::Reset() {
    photonTimes.clear();
    tRef = 0;
    hasRef = false;
}

//----------------------------------------------------------------------//
// Stores the arrival time of one photon at a fiber end                 //
//----------------------------------------------------------------------//
void NanowireReadout::AddPhoton(int det, int layer, int fiber, int side, double t_ns) {
    photonTimes[std::make_tuple(det, layer, fiber, side)].push_back(t_ns);
}

//----------------------------------------------------------------------//
// Converts the photons of each fiber end into nanowire current [nA]    //
// and writes one ntuple row per fiber end                              //
//----------------------------------------------------------------------//
void NanowireReadout::FillNtuple(int ntupleId, int eventNumber) {
    if(!hasRef) return;

    auto man = G4AnalysisManager::Instance();
    std::vector<double> counts(nTime);
    std::vector<double> I(nTime);

    for(const auto& [key, times] : photonTimes) {
        std::fill(counts.begin(), counts.end(), 0.);
        std::fill(I.begin(), I.end(), 0.);

        //Histogram: bin k = [k*dt, (k+1)*dt), times relative to the event reference
        int nPhotons = 0;
        for(double t : times) {
            long k = (long)std::floor((t - tRef + tOffset) / dt);
            if(k < 0 || k >= nTime) continue;
            counts[k] += 1;
            nPhotons++;
        }
        if(nPhotons == 0 || nPhotons < minPhotons) continue;

        //Convolution with the kernel, only over the bins that have photons
        for(int k = 0; k < nTime; k++) {
            if(counts[k] == 0) continue;
            for(int n = k; n < nTime; n++) I[n] += counts[k] * kernel[n - k];
        }
        for(int n = 0; n < nTime; n++) current[n] = (float)I[n];

        man->FillNtupleIColumn(ntupleId, 0, eventNumber);
        man->FillNtupleIColumn(ntupleId, 1, std::get<0>(key));   // det
        man->FillNtupleIColumn(ntupleId, 2, std::get<1>(key));   // layer
        man->FillNtupleIColumn(ntupleId, 3, std::get<2>(key));   // fiber
        man->FillNtupleIColumn(ntupleId, 4, std::get<3>(key));   // side
        man->FillNtupleIColumn(ntupleId, 5, nPhotons);           // photons inside the window
        man->FillNtupleDColumn(ntupleId, 6, tRef);
        // Column 7 ("current") is read automatically from the bound vector `current`
        man->AddNtupleRow(ntupleId);
    }
}
