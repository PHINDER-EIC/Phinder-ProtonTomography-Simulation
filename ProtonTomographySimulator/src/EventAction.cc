#include "EventAction.hh"
#include "EventActionMessenger.hh"

#include "G4AnalysisManager.hh"

#include <algorithm>
#include <cfloat>

#include "Analysis.hh"
#include "G4Run.hh"
#include "G4Event.hh"
#include "G4EventManager.hh"
#include "G4HCofThisEvent.hh"
#include "G4VHitsCollection.hh"
#include "G4TrajectoryContainer.hh"
#include "G4Trajectory.hh"
#include "G4VVisManager.hh"
#include "G4SDManager.hh"
#include "G4UImanager.hh"
#include "G4ios.hh"
#include "NanowireReadout.hh"

#include "FiberSensorHit.hh"

#include "ConfigurationGeometry.hh"

#include <math.h>

#include <iostream>
#include <map>

#define PI 3.141592653589793238462643383279502884



//----------------------------------------------------------------------//
// Constructor                                                          //
//----------------------------------------------------------------------//
EventAction::EventAction(ConfigurationGeometry *geom_, NanowireReadout *readout_) {

    geom = geom_;
    readout = readout_;
    G4String colName;
    G4SDManager* SDman = G4SDManager::GetSDMpointer();
    G4int numberOfDetectors = SDman->GetCollectionCapacity();
    SDman->ListTree();
    MyRndEngine = CLHEP::HepRandom::getTheEngine();
    myGauss = new CLHEP::RandGauss(MyRndEngine);

    for(auto i : geom->collections) {
        DHCID.push_back(SDman->GetCollectionID(i));
    }
    messenger = new EventActionMessenger(this);

}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//


//----------------------------------------------------------------------//
// Destructor                                                           //
//----------------------------------------------------------------------//
EventAction::~EventAction() {
    delete messenger;
}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//



//----------------------------------------------------------------------//
// Begin of the event                                                   //
//----------------------------------------------------------------------//
void EventAction::BeginOfEventAction(const G4Event*) {
    readout->Reset();
}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//


//----------------------------------------------------------------------//
// End of the event -> the action goes here                             //
//----------------------------------------------------------------------//
void EventAction::EndOfEventAction(const G4Event* evt) {


    auto man = G4AnalysisManager::Instance();
    G4HCofThisEvent * HCE = evt->GetHCofThisEvent();
    
    std::vector<FiberSensorHitsCollections> DHCs;
    //G4SDManager* SDman = G4SDManager::GetSDMpointer();
    if(HCE) {
        G4double tMin = DBL_MAX;
        G4bool hayHits = false;
        for (auto i : DHCID) {
            FiberSensorHitsCollection *a = 0;
            a = (FiberSensorHitsCollection*)(HCE->GetHC(i));
            FiberSensorHitsCollections DHCcoll;
            DHCcoll.push_back(a);
            DHCs.push_back(DHCcoll);
        }
        for (auto i : DHCs) {
            if(i.at(0)) {
                G4int n_hit = i.at(0)->entries();
                for(G4int hit = 0; hit < n_hit; hit++) {
                    FiberSensorHit* aHit = (*(i.at(0)))[hit];
                    if(aHit->GetEnergy() == 0) continue;
                    tMin = std::min(tMin, aHit->GetTime()/CLHEP::ns);
                    hayHits = true;
                    auto detID = aHit->GetDetectorID();
                    auto layerID = aHit->GetLayerID();
                    auto fiberID = aHit->GetFiberID();
                    man->FillNtupleIColumn(0, aHit->GetEventNumber());
                    man->FillNtupleIColumn(1, aHit->GetDetectorID());
                    man->FillNtupleIColumn(2, aHit->GetLayerID());
                    man->FillNtupleIColumn(3, aHit->GetFiberID());
                    man->FillNtupleDColumn(4, aHit->GetEnergy()/CLHEP::MeV);
                    man->FillNtupleDColumn(5, aHit->GetGenEnergy()/CLHEP::MeV);
                    man->FillNtupleDColumn(6, aHit->GetStepLength()/CLHEP::cm);
                    man->FillNtupleDColumn(7, aHit->GetTime()/CLHEP::ns);
                    man->FillNtupleDColumn(8, aHit->GetLocalPos().x()/CLHEP::cm);
                    man->FillNtupleDColumn(9, aHit->GetLocalPos().y()/CLHEP::cm);                       
                    man->FillNtupleDColumn(10, aHit->GetLocalPos().z()/CLHEP::cm);
                    man->FillNtupleDColumn(11, aHit->GetGlobalPos().x()/CLHEP::cm);
                    man->FillNtupleDColumn(12, aHit->GetGlobalPos().y()/CLHEP::cm);                       
                    man->FillNtupleDColumn(13, aHit->GetGlobalPos().z()/CLHEP::cm);
                    man->FillNtupleIColumn(14, aHit->GetGenID());
                    man->FillNtupleIColumn(15, aHit->GetGenTrackID());
                    man->AddNtupleRow(); 
                }
            }
        }
        if (hayHits) {readout->SetReferenceTime(tMin); readout->FillNtuple(3, evt->GetEventID()); }
    }
}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//
