// Reads a ROOT file from our simulation and writes, for every photon leaving a fiber end, the three times that
// build the photon peak (plotPulse.py):
//   t_cross : first step of the primary proton in that layer (bunch spread + proton time of flight)
//   t_born  : creation of the photon (t_born - t_cross = scintillation emission delay)
//   t_exit  : photon leaving the fiber end (t_exit - t_born = propagation along the fiber)
// Output: <prefix>_pulse.csv, one photon per row. Photons in layers not crossed by the primary are skipped.
//
// Usage: root -l -b -q 'exportPulse.C+("out.root", "bunches/run", firstEvent, nEvents)'

#include <map>
#include <set>
#include <cstdio>
#include "TFile.h"
#include "TTree.h"
#include "TString.h"

// Creation of one photon (from the "photons" tree)
struct PhotonBirth {
    double time;   // ns
    double dirz;   // direction along the fiber axis at creation
    int parentID;  // 1 = created by the primary proton, > 1 = by a secondary (delta electrons...)
};

void exportPulse(const char* inputFile, const char* prefix, int firstEvent = 0, int nEvents = 1)
{
    TFile file(inputFile);
    TTree* hits = (TTree*)file.Get("hits");
    TTree* photons = (TTree*)file.Get("photons");
    TTree* exitPhotons = (TTree*)file.Get("exitPhotons");
    if (!hits || !photons || !exitPhotons) { printf("Missing trees in %s\n", inputFile); return; }
    auto inChunk = [&](int ev) { return ev >= firstEvent && ev < firstEvent + nEvents; };

    // ---------------------------------------------------------------
    // 1. "hits": crossing time of the primary in each layer
    // ---------------------------------------------------------------
    hits->SetBranchStatus("*", 0);
    for (auto b : {"eventNumber", "layer", "genTrackID", "time"}) hits->SetBranchStatus(b, 1);
    int eventNumber, layer, genTrackID;
    double time;
    hits->SetBranchAddress("eventNumber", &eventNumber);
    hits->SetBranchAddress("layer", &layer);
    hits->SetBranchAddress("genTrackID", &genTrackID);
    hits->SetBranchAddress("time", &time);

    std::map<std::pair<int, int>, double> tCross;   // (eventNumber, layer) -> earliest step of the primary
    for (Long64_t i = 0; i < hits->GetEntries(); i++) {
        hits->GetEntry(i);
        if (!inChunk(eventNumber) || genTrackID != 1) continue;
        auto key = std::make_pair(eventNumber, layer);
        auto it = tCross.find(key);
        if (it == tCross.end() || time < it->second) tCross[key] = time;
    }

    // ---------------------------------------------------------------
    // 2. "exitPhotons": which photons leave the fiber (only those are looked up in "photons")
    // ---------------------------------------------------------------
    exitPhotons->SetBranchStatus("*", 0);
    for (auto b : {"eventNumber", "det", "layer", "fiber", "side", "trackID", "time"}) exitPhotons->SetBranchStatus(b, 1);
    int det, fiber, side, trackID;
    exitPhotons->SetBranchAddress("eventNumber", &eventNumber);
    exitPhotons->SetBranchAddress("det", &det);
    exitPhotons->SetBranchAddress("layer", &layer);
    exitPhotons->SetBranchAddress("fiber", &fiber);
    exitPhotons->SetBranchAddress("side", &side);
    exitPhotons->SetBranchAddress("trackID", &trackID);
    exitPhotons->SetBranchAddress("time", &time);

    std::set<std::pair<int, int>> exited;   // (eventNumber, trackID)
    for (Long64_t i = 0; i < exitPhotons->GetEntries(); i++) {
        exitPhotons->GetEntry(i);
        if (inChunk(eventNumber)) exited.insert({eventNumber, trackID});
    }

    // ---------------------------------------------------------------
    // 3. "photons": creation time of the photons that leave the fiber
    // ---------------------------------------------------------------
    photons->SetBranchStatus("*", 0);
    for (auto b : {"eventNumber", "trackID", "parentID", "time", "dirz"}) photons->SetBranchStatus(b, 1);
    int parentID;
    double dirz;
    photons->SetBranchAddress("eventNumber", &eventNumber);
    photons->SetBranchAddress("trackID", &trackID);
    photons->SetBranchAddress("parentID", &parentID);
    photons->SetBranchAddress("time", &time);
    photons->SetBranchAddress("dirz", &dirz);

    std::map<std::pair<int, int>, PhotonBirth> births;
    for (Long64_t i = 0; i < photons->GetEntries(); i++) {
        photons->GetEntry(i);
        if (!inChunk(eventNumber)) continue;
        auto key = std::make_pair(eventNumber, trackID);
        if (exited.count(key)) births[key] = {time, dirz, parentID};
    }

    // ---------------------------------------------------------------
    // 4. Second loop over "exitPhotons": write the three times of every photon
    // ---------------------------------------------------------------
    FILE* out = fopen(Form("%s_pulse.csv", prefix), "w");
    fprintf(out, "event,det,layer,fiber,side,t_cross_ns,t_born_ns,t_exit_ns,reflected,from_primary\n");
    long nPhotons = 0;
    for (Long64_t i = 0; i < exitPhotons->GetEntries(); i++) {
        exitPhotons->GetEntry(i);
        if (!inChunk(eventNumber)) continue;
        auto c = tCross.find({eventNumber, layer});
        auto b = births.find({eventNumber, trackID});
        if (c == tCross.end() || b == births.end()) continue;
        int reflected = ((b->second.dirz > 0) != (side > 0)) ? 1 : 0;   // born going to the other end: reflected there
        fprintf(out, "%d,%d,%d,%d,%d,%.5f,%.5f,%.5f,%d,%d\n", eventNumber, det, layer, fiber, side,
                c->second, b->second.time, time, reflected, b->second.parentID == 1 ? 1 : 0);
        nPhotons++;
    }
    fclose(out);
    printf("events %d-%d: primary layer crossings = %zu, photons = %ld\n", firstEvent, firstEvent + nEvents - 1, tCross.size(), nPhotons);
}
