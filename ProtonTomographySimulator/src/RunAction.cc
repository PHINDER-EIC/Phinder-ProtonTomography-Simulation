#include "RunAction.hh"
#include "G4AnalysisManager.hh"

#include "Analysis.hh"
#include "NanowireReadout.hh"

#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4UnitsTable.hh"



//----------------------------------------------------------------------//
// Constructor                                                          //
//----------------------------------------------------------------------//
RunAction::RunAction(std::string name, NanowireReadout *readout_) {
    nameOfOutputFile = name;
    readout = readout_;
}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//


//----------------------------------------------------------------------//
// Destructor                                                           //
//----------------------------------------------------------------------//
RunAction::~RunAction() {}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//


//----------------------------------------------------------------------//
// We set up the columns when the run starts                            //
//----------------------------------------------------------------------//
void RunAction::BeginOfRunAction(const G4Run* run) {

    G4AnalysisManager* man = G4AnalysisManager::Instance();
    man->OpenFile(nameOfOutputFile.c_str());
    // Create ntuple
    man->CreateNtuple("hits", "The hits");
    
    man->CreateNtupleIColumn("eventNumber");
    man->CreateNtupleIColumn("det");
    man->CreateNtupleIColumn("layer");
    man->CreateNtupleIColumn("fiber");
    man->CreateNtupleDColumn("energy");
    man->CreateNtupleDColumn("kineticEnergy");
    man->CreateNtupleDColumn("stepLength");
    man->CreateNtupleDColumn("time");
    man->CreateNtupleDColumn("localx");
    man->CreateNtupleDColumn("localy");
    man->CreateNtupleDColumn("localz");
    man->CreateNtupleDColumn("x");
    man->CreateNtupleDColumn("y");
    man->CreateNtupleDColumn("z");
    man->CreateNtupleIColumn("genID");
    man->CreateNtupleIColumn("genTrackID");
    man->FinishNtuple();


    // Ntuple 1: optical photons born in the fiber cores
    man->CreateNtuple("photons", "Optical photons at creation");
    man->CreateNtupleIColumn("eventNumber");
    man->CreateNtupleIColumn("det");
    man->CreateNtupleIColumn("layer");
    man->CreateNtupleIColumn("fiber");
    man->CreateNtupleIColumn("process");
    man->CreateNtupleIColumn("trackID");
    man->CreateNtupleIColumn("parentID");
    man->CreateNtupleDColumn("x");
    man->CreateNtupleDColumn("y");
    man->CreateNtupleDColumn("z");
    man->CreateNtupleDColumn("localx");
    man->CreateNtupleDColumn("localy");
    man->CreateNtupleDColumn("localz");
    man->CreateNtupleDColumn("dirx");
    man->CreateNtupleDColumn("diry");
    man->CreateNtupleDColumn("dirz");
    man->CreateNtupleDColumn("polx");
    man->CreateNtupleDColumn("poly");
    man->CreateNtupleDColumn("polz");
    man->CreateNtupleDColumn("energy");
    man->CreateNtupleDColumn("wavelength");
    man->CreateNtupleDColumn("time");
    man->FinishNtuple();

    // Ntuple 2: optical photons leaving the fibers
    man->CreateNtuple("exitPhotons", "Optical photons leaving the fibers");
    man->CreateNtupleIColumn("eventNumber");
    man->CreateNtupleIColumn("det");
    man->CreateNtupleIColumn("layer");
    man->CreateNtupleIColumn("fiber");
    man->CreateNtupleIColumn("trackID");
    man->CreateNtupleIColumn("parentID");
    man->CreateNtupleIColumn("process");
    man->CreateNtupleIColumn("exitVolume");   // 0 = core, 1 = cladding
    man->CreateNtupleIColumn("side");         // +1 / -1 fiber end
    man->CreateNtupleDColumn("x");
    man->CreateNtupleDColumn("y");
    man->CreateNtupleDColumn("z");
    man->CreateNtupleDColumn("localx");
    man->CreateNtupleDColumn("localy");
    man->CreateNtupleDColumn("localz");
    man->CreateNtupleDColumn("dirInx");
    man->CreateNtupleDColumn("dirIny");
    man->CreateNtupleDColumn("dirInz");
    man->CreateNtupleDColumn("dirOutx");
    man->CreateNtupleDColumn("dirOuty");
    man->CreateNtupleDColumn("dirOutz");
    man->CreateNtupleDColumn("polx");
    man->CreateNtupleDColumn("poly");
    man->CreateNtupleDColumn("polz");
    man->CreateNtupleDColumn("energy");
    man->CreateNtupleDColumn("wavelength");
    man->CreateNtupleDColumn("time");
    man->CreateNtupleDColumn("trackLength");
    man->FinishNtuple();

    // Ntuple 3: current at fiber's ends
    man->CreateNtuple("nwCurrent", "Nanowire current per fiber end");
    man->CreateNtupleIColumn("eventNumber");
    man->CreateNtupleIColumn("det");
    man->CreateNtupleIColumn("layer");
    man->CreateNtupleIColumn("fiber");
    man->CreateNtupleIColumn("side");
    man->CreateNtupleIColumn("nPhotons");
    man->CreateNtupleDColumn("tRef");
    man->CreateNtupleFColumn("current", readout->CurrentBuffer());   // vector<float>, nTime muestras [nA]
    man->FinishNtuple();

}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//


//----------------------------------------------------------------------//
// At the end we store                                                  //
//----------------------------------------------------------------------//
void RunAction::EndOfRunAction(const G4Run* aRun)
{
    G4AnalysisManager* analysisManager = G4AnalysisManager::Instance();
    analysisManager->Write();
    analysisManager->CloseFile();
    //delete G4AnalysisManager::Instance();
    
}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//

