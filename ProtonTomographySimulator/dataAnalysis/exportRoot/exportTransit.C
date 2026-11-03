// Reads a ROOT file from our simulation and writes the transit of the primary proton through every fiber core it crosses
// (plotTransit.py). Geant4 usually crosses the core in a single step, so the transit time is not the difference between
// hit times: it is computed as sum(stepLength / v), with v from the kinetic energy at the start of each step.
// Output: <prefix>_transit.csv, one row per fiber crossed by the primary.
//
// Usage: root -l -b -q 'exportTransit.C+("out.root", "bunches/run", nEvents)'

#include <map>
#include <tuple>
#include <cmath>
#include <cstdio>
#include "TFile.h"
#include "TTree.h"
#include "TString.h"

// Accumulated transit of the primary through one fiber core
struct Transit {
    int det;
    double tEntry;   // ns, first step
    double kinE;     // MeV, kinetic energy at the first step
    double path;     // cm, sum of the step lengths
    double time;     // ns, sum of stepLength / v
    int nSteps;
};

void exportTransit(const char* inputFile, const char* prefix, int nEvents = 100)
{
    const double protonMass = 938.272;   // MeV
    const double cLight = 29.9792458;    // cm/ns

    TFile file(inputFile);
    TTree* hits = (TTree*)file.Get("hits");
    if (!hits) { printf("No 'hits' tree in %s\n", inputFile); return; }

    hits->SetBranchStatus("*", 0);
    for (auto b : {"eventNumber", "det", "layer", "fiber", "genTrackID", "time", "kineticEnergy", "stepLength"}) hits->SetBranchStatus(b, 1);
    int eventNumber, det, layer, fiber, genTrackID;
    double time, kineticEnergy, stepLength;
    hits->SetBranchAddress("eventNumber", &eventNumber);
    hits->SetBranchAddress("det", &det);
    hits->SetBranchAddress("layer", &layer);
    hits->SetBranchAddress("fiber", &fiber);
    hits->SetBranchAddress("genTrackID", &genTrackID);
    hits->SetBranchAddress("time", &time);
    hits->SetBranchAddress("kineticEnergy", &kineticEnergy);
    hits->SetBranchAddress("stepLength", &stepLength);

    std::map<std::tuple<int, int, int>, Transit> transits;   // (eventNumber, layer, fiber)
    for (Long64_t i = 0; i < hits->GetEntries(); i++) {
        hits->GetEntry(i);
        if (eventNumber >= nEvents || genTrackID != 1) continue;   // only the primary proton
        double gamma = 1.0 + kineticEnergy / protonMass;
        double v = cLight * std::sqrt(1.0 - 1.0 / (gamma * gamma));
        auto key = std::make_tuple(eventNumber, layer, fiber);
        auto it = transits.find(key);
        if (it == transits.end()) {
            transits[key] = {det, time, kineticEnergy, stepLength, stepLength / v, 1};
            continue;
        }
        Transit& tr = it->second;
        if (time < tr.tEntry) { tr.tEntry = time; tr.kinE = kineticEnergy; }   // keep the entry step
        tr.path += stepLength;
        tr.time += stepLength / v;
        tr.nSteps++;
    }

    FILE* out = fopen(Form("%s_transit.csv", prefix), "w");
    fprintf(out, "event,det,layer,fiber,kinE_MeV,beta,path_mm,transit_ps,n_steps\n");
    for (auto& [key, tr] : transits) {
        double gamma = 1.0 + tr.kinE / protonMass;
        fprintf(out, "%d,%d,%d,%d,%.3f,%.5f,%.4f,%.3f,%d\n", std::get<0>(key), tr.det, std::get<1>(key), std::get<2>(key),
                tr.kinE, std::sqrt(1.0 - 1.0 / (gamma * gamma)), 10 * tr.path, 1000 * tr.time, tr.nSteps);
    }
    fclose(out);
    printf("primary fiber crossings = %zu\n", transits.size());
}
