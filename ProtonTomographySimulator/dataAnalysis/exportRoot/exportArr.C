// Reads a ROOT file from our simulation and writes, for Carlo's nanowire simulator, the whole device:
//   <prefix>_arrivals.csv   : one photon per row (event, channel, arrival_time_ns), all detectors/layers/fibers
//   <prefix>_truth.csv      : one row per event (proton)
//   <prefix>_hits_truth.csv : one row per fiber crossed by the primary proton (Monte Carlo truth)
//   <prefix>_channels.csv   : channel -> (det, layer, fiber, side) map
// channel = (layer * NFibers + fiber) * 2 + (side > 0 ? 1 : 0), so 8 layers x 27 fibers x 2 ends = 432 channels.
// All times are relative to the first hit of the event + tOffset.
//
// Usage: root -l -b -q 'exportArr.C+("out.root", "bridge/all_000", firstEvent, nEvents)'
// Then:  nanowire_detector_stage7.py convert --prefix bridge/all_000 --dt-ns 0.005 --tmax-ns 20 --channels 432
// CUIDADO CON LA RAM

#include <map>
#include <cstdio>
#include <cmath>
#include <utility>
#include <set>
#include "TFile.h"
#include "TTree.h"
#include "TString.h"

const int NLayers = 8;
const int NFibers = 27;
const int NChannels = NLayers * NFibers * 2;   // 2 ends per fiber

// Each fiber end of the device gets a fixed channel number, the same in every event
int channelOf(int layer, int fiber, int side) { return (layer * NFibers + fiber) * 2 + (side > 0 ? 1 : 0); }

// Data from the first step of the primary proton in one fiber
struct PrimaryHit {
    int det;
    int layer;
    int fiber;
    double time;     // ns
    double localz;   // cm, location along the fiber (0 = center)
    double kinE;     // MeV
};

void exportArr(const char* inputFile, const char* prefix, int firstEvent = 0, int nEvents = 100, double tOffset = 5.0)
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

    std::map<int, double> tRef;   // eventNumber -> earliest hit time of any particle in the event
    std::map<std::pair<int, int>, PrimaryHit> primaryHits;   // (eventNumber, fiberKey) -> first step of the primary in that fiber
    for (Long64_t i = 0; i < hits->GetEntries(); i++) {
        hits->GetEntry(i);
        if (eventNumber < firstEvent || eventNumber >= firstEvent + nEvents) continue;   // only this chunk
        auto r = tRef.find(eventNumber);
        if (r == tRef.end() || time < r->second) tRef[eventNumber] = time;               // earliest hit, any particle

        if (genTrackID != 1) continue;                                   // truth: only the primary proton
        auto key = std::make_pair(eventNumber, fiberKey(det, layer, fiber));
        auto it = primaryHits.find(key);
        if (it != primaryHits.end() && it->second.time <= time) continue; // keep the earliest step in each fiber
        primaryHits[key] = {det, layer, fiber, time, localz, kineticEnergy};
    }


    // ---------------------------------------------------------------
    // Write truth.csv and renumber the selected events 0, 1, 2...
    // ---------------------------------------------------------------
    // Renumber the events of this chunk 0, 1, 2... (only events with at least one hit: the rest have no light)
    std::map<int, int> newIndex;   // eventNumber from Geant4 -> new index
    for (auto& [g4Event, t0] : tRef) {
        int k = newIndex.size();
        newIndex[g4Event] = k;
    }

    // truth.csv: one row per event. Carlo only uses it to count events
    FILE* truth = fopen(Form("%s_truth.csv", prefix), "w");
    fprintf(truth, "event,g4_event,t_ref_ns\n");
    for (auto& [g4Event, k] : newIndex)
        fprintf(truth, "%d,%d,%.5f\n", k, g4Event, tRef[g4Event]);
    fclose(truth);

    // hits_truth.csv: one row per fiber crossed by the primary, times on the same clock as arrivals.csv
    FILE* hitsTruth = fopen(Form("%s_hits_truth.csv", prefix), "w");
    fprintf(hitsTruth, "event,det,layer,fiber,channel_minus,channel_plus,t_hit_ns,localz_cm,kinE_MeV,dt_true_ns,label\n");
    for (auto& [key, hit] : primaryHits) {
        int g4Event = key.first;
        int chMinus = channelOf(hit.layer, hit.fiber, -1);
        double tHit = hit.time - tRef[g4Event] + tOffset;
        double dtTrue = 2 * fabs(hit.localz) / vFiber;
        int label = (hit.localz > 0) ? 1 : 0;
        fprintf(hitsTruth, "%d,%d,%d,%d,%d,%d,%.5f,%.5f,%.4f,%.5f,%d\n",
                newIndex[g4Event], hit.det, hit.layer, hit.fiber, chMinus, chMinus + 1,
                tHit, hit.localz, hit.kinE, dtTrue, label);
    }
    fclose(hitsTruth);

    // channels.csv: the map is the same for every chunk
    FILE* map = fopen(Form("%s_channels.csv", prefix), "w");
    fprintf(map, "channel,det,layer,fiber,side\n");
    for (int l = 0; l < NLayers; l++)
        for (int f = 0; f < NFibers; f++)
            for (int s : {-1, +1})
                fprintf(map, "%d,%d,%d,%d,%d\n", channelOf(l, f, s), l / 4, l, f, s);
    fclose(map);


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
        auto it = newIndex.find(eventNumber);                      // event in this chunk?
        if (it == newIndex.end()) continue;
        if (layer < 0 || layer >= NLayers || fiber < 0 || fiber >= NFibers) {   // geometry differs from NLayers/NFibers
            printf("WARNING: photon in layer %d fiber %d outside the channel map, skipped\n", layer, fiber);
            continue;
        }
        int channel = channelOf(layer, fiber, side);
        double tArrival = time - tRef[eventNumber] + tOffset;      // common clock for the whole event
        fprintf(arrivals, "%d,%d,%.5f\n", it->second, channel, tArrival);
        nPhotons++;
    }

    fclose(arrivals);

    printf("events = %zu, primary fiber crossings = %zu, photons = %ld\n", newIndex.size(), primaryHits.size(), nPhotons);
}
