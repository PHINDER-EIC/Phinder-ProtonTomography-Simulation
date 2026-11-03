// Reads a ROOT file from our simulation and writes two CSV files for Carlo's nanowire simulator:
//   <prefix>_arrivals.csv : one photon per row (event, channel, arrival_time_ns)
//   <prefix>_truth.csv    : one row per event with the Monte Carlo truth
// Only the fibers crossed by the primary proton (genTrackID == 1) are used.
//
// Usage: root -l -b -q 'exportArr.C+("output_file_from_g4.root", "prefix_of_output_files", int: max_events)'
// These files will be used by Carlo's simulator to generate the output of the nanowire detector, which will be compared with the Monte Carlo truth.

#include <map>
#include <cstdio>
#include <cmath>
#include <utility>
#include <set>
#include "TFile.h"
#include "TTree.h"
#include "TString.h"

// Data from the first step of the primary proton in one fiber
struct PrimaryHit {
    int det;
    int layer;
    int fiber;
    double time;     // ns
    double localz;   // cm, location along the fiber (0 = center)
    double kinE;     // MeV
};

void exportArr(const char* inputFile, const char* prefix, int maxEvents = 200, double tOffset = 5.0)
{
    const double vFiber = 18.7;   // cm/ns, velocidad efectiva de la luz en la fibra (medida en nuestra simulación)

    TFile file(inputFile);
    TTree* hits = (TTree*)file.Get("hits");
    TTree* exitPhotons = (TTree*)file.Get("exitPhotons");

    // ---------------------------------------------------------------
    // 1. Loop over "hits": save, per event, the first step of the primary in each fiber it crosses 
    // ---------------------------------------------------------------
    int eventNumber, det, layer, fiber, genTrackID;
    double time, localz, kineticEnergy;
    hits->SetBranchAddress("eventNumber", &eventNumber);
    hits->SetBranchAddress("det", &det);
    hits->SetBranchAddress("layer", &layer);
    hits->SetBranchAddress("fiber", &fiber);
    hits->SetBranchAddress("genTrackID", &genTrackID);
    hits->SetBranchAddress("time", &time);
    hits->SetBranchAddress("localz", &localz);
    hits->SetBranchAddress("kineticEnergy", &kineticEnergy);

    // Same encoding as the Geant4 copy number: det*100000 + layer*1000 + fiber
    auto fiberKey = [](int det, int layer, int fiber) { return det * 100000 + layer * 1000 + fiber; };

    std::map<std::pair<int, int>, PrimaryHit> primaryHits;   // (eventNumber, fiberKey) -> first step of the primary in that fiber
    for (Long64_t i = 0; i < hits->GetEntries(); i++) {
        hits->GetEntry(i);
        if (genTrackID != 1) continue;                                   // only the primary proton, all layers
        auto key = std::make_pair(eventNumber, fiberKey(det, layer, fiber));
        auto it = primaryHits.find(key);
        if (it != primaryHits.end() && it->second.time <= time) continue; // keep the earliest step in each fiber
        primaryHits[key] = {det, layer, fiber, time, localz, kineticEnergy};
    }

    // ---------------------------------------------------------------
    // Write truth.csv and renumber the selected events 0, 1, 2...
    // ---------------------------------------------------------------
    std::map<std::pair<int, int>, int> newIndex;   // (eventNumber, fiberKey) -> new index 0, 1, 2...
    std::set<int> selectedG4Events;
    FILE* truth = fopen(Form("%s_truth.csv", prefix), "w");
    fprintf(truth, "event,g4_event,det,layer,fiber,t_hit_ns,localz_cm,kinE_MeV,dt_true_ns,label\n");
    for (auto& [key, hit] : primaryHits) {
        int g4Event = key.first;
        if (!selectedG4Events.count(g4Event)) {
            if ((int)selectedG4Events.size() >= maxEvents) break;   // maxEvents counts protons
            selectedG4Events.insert(g4Event);
        }
        int k = newIndex.size();
        newIndex[key] = k;
        double dtTrue = 2 * fabs(hit.localz) / vFiber;
        int label = (hit.localz > 0) ? 1 : 0;
        fprintf(truth, "%d,%d,%d,%d,%d,%.5f,%.5f,%.4f,%.5f,%d\n",
                k, g4Event, hit.det, hit.layer, hit.fiber, hit.time, hit.localz, hit.kinE, dtTrue, label);
    }
    fclose(truth);

    // ---------------------------------------------------------------
    // 3. Loop over "exitPhotons" and write arrivals.csv
    // ---------------------------------------------------------------
    // exitPhotons has millions of rows and ~30 branches: reading only the necessary ones makes it much faster
    exitPhotons->SetBranchStatus("*", 0);
    for (auto b : {"eventNumber", "det", "layer", "fiber", "side", "time"}) exitPhotons->SetBranchStatus(b, 1);

    int side;
    exitPhotons->SetBranchAddress("eventNumber", &eventNumber);
    exitPhotons->SetBranchAddress("det", &det);
    exitPhotons->SetBranchAddress("layer", &layer);
    exitPhotons->SetBranchAddress("fiber", &fiber);
    exitPhotons->SetBranchAddress("side", &side);
    exitPhotons->SetBranchAddress("time", &time);

    FILE* arrivals = fopen(Form("%s_arrivals.csv", prefix), "w");
    fprintf(arrivals, "event,channel,arrival_time_ns\n");
    long nPhotons = 0;
    for (Long64_t i = 0; i < exitPhotons->GetEntries(); i++) {
        exitPhotons->GetEntry(i);
        auto key = std::make_pair(eventNumber, fiberKey(det, layer, fiber));
        auto it = newIndex.find(key);                              // fiber crossed by the primary in a selected event?
        if (it == newIndex.end()) continue;
        const PrimaryHit& hit = primaryHits[key];
        int channel = (side > 0) ? 1 : 0;                          // channel 0 = side -, channel 1 = side +
        double tArrival = time - hit.time + tOffset;               // relative to the crossing of THIS fiber
        fprintf(arrivals, "%d,%d,%.5f\n", it->second, channel, tArrival);
        nPhotons++;
    }
    fclose(arrivals);

    printf("protons = %zu, fiber crossings = %zu,  photons = %ld\n", selectedG4Events.size(), newIndex.size(), nPhotons);
}
