// Reads a ROOT file from our simulation and writes the absolute times needed for the bunch plots (plotBunches.py):
//   <prefix>_primary.csv : one row per layer crossed by the primary proton (first step in that layer)
//   <prefix>_photons.csv : one row per photon leaving a fiber end (optional, it can be large)
// Unlike exportArr.C, no per-event reference is subtracted: times are the Geant4 global time, so they keep
// the bunch time spread given by tBeamSigma in the json file.
//
// Usage: root -l -b -q 'exportBunches.C+("out.root", "bunches/run", nEvents, writePhotons)'

#include <map>
#include <tuple>
#include <cstdio>
#include "TFile.h"
#include "TTree.h"
#include "TString.h"

// Data from the first step of the primary proton in one layer
struct PrimaryHit {
    int det;
    int fiber;
    double time;   // ns
};

void exportBunches(const char* inputFile, const char* prefix, int nEvents = 100, bool writePhotons = true)
{
    TFile file(inputFile);
    TTree* hits = (TTree*)file.Get("hits");
    TTree* exitPhotons = (TTree*)file.Get("exitPhotons");
    if (!hits || !exitPhotons) { printf("No 'hits' or 'exitPhotons' tree in %s\n", inputFile); return; }

    // ---------------------------------------------------------------
    // 1. Loop over "hits": first step of the primary in each layer it crosses
    // ---------------------------------------------------------------
    hits->SetBranchStatus("*", 0);
    for (auto b : {"eventNumber", "det", "layer", "fiber", "genTrackID", "time"}) hits->SetBranchStatus(b, 1);

    int eventNumber, det, layer, fiber, genTrackID;
    double time;
    hits->SetBranchAddress("eventNumber", &eventNumber);
    hits->SetBranchAddress("det", &det);
    hits->SetBranchAddress("layer", &layer);
    hits->SetBranchAddress("fiber", &fiber);
    hits->SetBranchAddress("genTrackID", &genTrackID);
    hits->SetBranchAddress("time", &time);

    // layer is already a global index (0-7), so (eventNumber, layer) identifies the plane
    std::map<std::pair<int, int>, PrimaryHit> primaryHits;
    for (Long64_t i = 0; i < hits->GetEntries(); i++) {
        hits->GetEntry(i);
        if (eventNumber >= nEvents || genTrackID != 1) continue;          // only the primary proton
        auto key = std::make_pair(eventNumber, layer);
        auto it = primaryHits.find(key);
        if (it != primaryHits.end() && it->second.time <= time) continue; // keep the earliest step in each layer
        primaryHits[key] = {det, fiber, time};
    }

    FILE* primary = fopen(Form("%s_primary.csv", prefix), "w");
    fprintf(primary, "event,det,layer,fiber,t_ns\n");
    for (auto& [key, hit] : primaryHits)
        fprintf(primary, "%d,%d,%d,%d,%.5f\n", key.first, hit.det, key.second, hit.fiber, hit.time);
    fclose(primary);
    printf("primary layer crossings = %zu\n", primaryHits.size());

    if (!writePhotons) return;

    // ---------------------------------------------------------------
    // 2. Loop over "exitPhotons": arrival time of every photon at the fiber ends
    // ---------------------------------------------------------------
    exitPhotons->SetBranchStatus("*", 0);
    for (auto b : {"eventNumber", "det", "layer", "fiber", "side", "time"}) exitPhotons->SetBranchStatus(b, 1);

    int side;
    exitPhotons->SetBranchAddress("eventNumber", &eventNumber);
    exitPhotons->SetBranchAddress("det", &det);
    exitPhotons->SetBranchAddress("layer", &layer);
    exitPhotons->SetBranchAddress("fiber", &fiber);
    exitPhotons->SetBranchAddress("side", &side);
    exitPhotons->SetBranchAddress("time", &time);

    FILE* photons = fopen(Form("%s_photons.csv", prefix), "w");
    fprintf(photons, "event,det,layer,fiber,side,t_ns\n");
    long nPhotons = 0;
    for (Long64_t i = 0; i < exitPhotons->GetEntries(); i++) {
        exitPhotons->GetEntry(i);
        if (eventNumber >= nEvents) continue;
        fprintf(photons, "%d,%d,%d,%d,%d,%.5f\n", eventNumber, det, layer, fiber, side, time);
        nPhotons++;
    }
    fclose(photons);
    printf("photons = %ld\n", nPhotons);
}
