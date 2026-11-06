#include "SteppingAction.hh"
#include "G4AnalysisManager.hh"
#include "G4EventManager.hh"
#include "G4Event.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4VProcess.hh"
#include "G4ProcessManager.hh"
#include "G4OpBoundaryProcess.hh"
#include "G4VTouchable.hh"
#include "G4NavigationHistory.hh"
#include "G4OpticalPhoton.hh"
#include "G4SystemOfUnits.hh"
#include "G4PhysicalConstants.hh"
#include "NanowireReadout.hh"

//-----------------------------------------------------------------------//
// Optical device (index-matched) coupled to the fiber ends: photons     //
// reflected at a fiber end (core/cladding -> outside) are killed, so    //
// they cannot bounce and leave through the other end.                   //
// Comment this line and next one to allow reflections at the fiber ends.//
//-----------------------------------------------------------------------//
// #define KILL_REFLECTED_AT_FIBER_ENDS

//----------------------------------------------------------------------//
// Ideal index-matched coupling at the fiber ends: every photon that    //
// reaches a fiber end is stored and killed, whether Geant4 refracts or //
// reflects it at the fiber-air boundary (there is no real interface).  //
// When defined, KILL_REFLECTED_AT_FIBER_ENDS has no effect.            //
//----------------------------------------------------------------------//
#define INDEX_MATCHED_FIBER_ENDS

//----------------------------------------------------------------------//
// Constructor                                                          //
//----------------------------------------------------------------------//
SteppingAction::SteppingAction(NanowireReadout *readout_, G4bool storeExitPhotons_) {
    boundary = nullptr;
    readout = readout_;
    storeExitPhotons = storeExitPhotons_;
}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//


//----------------------------------------------------------------------//
// Destructor                                                           //
//----------------------------------------------------------------------//
SteppingAction::~SteppingAction() {}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//


//----------------------------------------------------------------------//
// Optical photons leaving a fiber (core or cladding) towards the       //
// outside are stored in the "exitPhotons" ntuple and then killed, as   //
// if an ideal photodetector were placed there                          //
//----------------------------------------------------------------------//
void SteppingAction::UserSteppingAction(const G4Step *step) {

    //Ignore all particles that are not optical photons
    G4Track *track = step->GetTrack();
    if(track->GetDefinition() != G4OpticalPhoton::Definition()) return;

    //The step must end on a geometrical boundary
    G4StepPoint *pre = step->GetPreStepPoint();
    G4StepPoint *post = step->GetPostStepPoint();
    if(post->GetStepStatus() != fGeomBoundary) return;

    G4VPhysicalVolume *preVol = pre->GetPhysicalVolume();
    G4VPhysicalVolume *postVol = post->GetPhysicalVolume();
    if(preVol == nullptr || postVol == nullptr) return;

    //From the core or the cladding to something that is not part of a fiber
    G4bool fromCore = G4StrUtil::starts_with(preVol->GetName(), "FiberCorePhys");
    G4bool fromClad = G4StrUtil::starts_with(preVol->GetName(), "FiberCladdingPhys");
    if(!fromCore && !fromClad) return; //If they come from somewhere outside the fiber
    if(G4StrUtil::starts_with(postVol->GetName(), "Fiber")) return; //If they go into a fiber

    //The photon must be transmitted, not reflected, at the boundary (We only want Fresnel refraction)
    if(boundary == nullptr) {
        G4ProcessVector *procs = track->GetDefinition()->GetProcessManager()->GetProcessList();
        for(size_t i = 0; i < procs->size(); i++) {
            if((*procs)[i]->GetProcessName() == "OpBoundary") { //Get the boundary process
                boundary = static_cast<G4OpBoundaryProcess*>((*procs)[i]); //Transform from G4VProcess to G4OpBoundaryProcess
                break;
            }
        }
    }
    if(boundary == nullptr) return;
#ifndef INDEX_MATCHED_FIBER_ENDS
    if(boundary->GetStatus() != FresnelRefraction) {
#ifdef KILL_REFLECTED_AT_FIBER_ENDS
        //The photon reached a fiber end but was reflected back (total internal or Fresnel reflection):
        //it is absorbed by the optical device coupled to the fiber end, so it is killed and not stored
        track->SetTrackStatus(fStopAndKill);
#endif
        return;
    }
#endif

    //Fiber identifiers from the copy number of the volume the photon leaves
    const G4VTouchable *touchable = pre->GetTouchable();
    G4int copyNumber = touchable->GetCopyNumber();
    G4int det = copyNumber / 100000;
    G4int layer = (copyNumber / 1000) % 100;
    G4int fiber = copyNumber % 1000;

    //Creator process: 0 = Scintillation, 1 = Cerenkov
    const G4VProcess *creator = track->GetCreatorProcess();
    G4int process = (creator != nullptr && creator->GetProcessName() == "Scintillation") ? 0 : 1;

    //Exit point (post-step, on the fiber face) and directions:
    //dirIn = inside the fiber (pre-step), dirOut = after refraction (post-step)
    const G4AffineTransform &toLocal = touchable->GetHistory()->GetTopTransform();
    G4ThreeVector pos = post->GetPosition();
    G4ThreeVector localPos = toLocal.TransformPoint(pos);
    G4ThreeVector dirIn = toLocal.TransformAxis(pre->GetMomentumDirection());
#ifdef INDEX_MATCHED_FIBER_ENDS
    G4ThreeVector dirOut = dirIn; //The photon is transmitted through the fiber end without refraction
    G4ThreeVector localPol = toLocal.TransformAxis(pre->GetPolarization());
#else
    G4ThreeVector dirOut = toLocal.TransformAxis(post->GetMomentumDirection());
    G4ThreeVector localPol = toLocal.TransformAxis(post->GetPolarization());
#endif
    G4double energy = post->GetKineticEnergy();

    //Fiber end through which the photon leaves: +1 (z>0) or -1 (z<0)
    G4int side = (localPos.z() > 0) ? 1 : -1;

    auto man = G4AnalysisManager::Instance();
    G4int id = 2;
    if(storeExitPhotons) {
    man->FillNtupleIColumn(id, 0, G4EventManager::GetEventManager()->GetConstCurrentEvent()->GetEventID());
    man->FillNtupleIColumn(id, 1, det);
    man->FillNtupleIColumn(id, 2, layer);
    man->FillNtupleIColumn(id, 3, fiber);
    man->FillNtupleIColumn(id, 4, track->GetTrackID());
    man->FillNtupleIColumn(id, 5, track->GetParentID());
    man->FillNtupleIColumn(id, 6, process);
    man->FillNtupleIColumn(id, 7, fromCore ? 0 : 1);
    man->FillNtupleIColumn(id, 8, side);
    man->FillNtupleDColumn(id, 9, pos.x()/CLHEP::cm);
    man->FillNtupleDColumn(id, 10, pos.y()/CLHEP::cm);
    man->FillNtupleDColumn(id, 11, pos.z()/CLHEP::cm);
    man->FillNtupleDColumn(id, 12, localPos.x()/CLHEP::cm);
    man->FillNtupleDColumn(id, 13, localPos.y()/CLHEP::cm);
    man->FillNtupleDColumn(id, 14, localPos.z()/CLHEP::cm);
    man->FillNtupleDColumn(id, 15, dirIn.x());
    man->FillNtupleDColumn(id, 16, dirIn.y());
    man->FillNtupleDColumn(id, 17, dirIn.z());
    man->FillNtupleDColumn(id, 18, dirOut.x());
    man->FillNtupleDColumn(id, 19, dirOut.y());
    man->FillNtupleDColumn(id, 20, dirOut.z());
    man->FillNtupleDColumn(id, 21, localPol.x());
    man->FillNtupleDColumn(id, 22, localPol.y());
    man->FillNtupleDColumn(id, 23, localPol.z());
    man->FillNtupleDColumn(id, 24, energy/CLHEP::eV);
    man->FillNtupleDColumn(id, 25, CLHEP::h_Planck*CLHEP::c_light/energy/CLHEP::nm);
    man->FillNtupleDColumn(id, 26, post->GetGlobalTime()/CLHEP::ns);
    man->FillNtupleDColumn(id, 27, track->GetTrackLength()/CLHEP::cm);
    man->AddNtupleRow(id);
    }

    readout->AddPhoton(det, layer, fiber, side, post->GetGlobalTime()/CLHEP::ns);

    //Ideal detector: the photon is absorbed once it leaves the fiber (comentar para simular los fotones fuera de la fibra)
    track->SetTrackStatus(fStopAndKill);
    
}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//
