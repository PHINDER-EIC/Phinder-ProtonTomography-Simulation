#include "StackingAction.hh"
#include "G4AnalysisManager.hh"
#include "G4EventManager.hh"
#include "G4Event.hh"
#include "G4Track.hh"
#include "G4VProcess.hh"
#include "G4VTouchable.hh"
#include "G4NavigationHistory.hh"
#include "G4OpticalPhoton.hh"
#include "G4SystemOfUnits.hh"
#include "G4PhysicalConstants.hh"

//----------------------------------------------------------------------//
// Constructor                                                          //
//----------------------------------------------------------------------//
StackingAction::StackingAction(G4bool kill, G4bool storePhotons_) {
    killOpticalPhotons = kill;
    storePhotons = storePhotons_;
}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//


//----------------------------------------------------------------------//
// Destructor                                                           //
//----------------------------------------------------------------------//
StackingAction::~StackingAction() {}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//


//----------------------------------------------------------------------//
// Called for every new track: optical photons born in a fiber core     //
// are stored in the "photons" ntuple                                   //
//----------------------------------------------------------------------//
G4ClassificationOfNewTrack StackingAction::ClassifyNewTrack(const G4Track *track) {

    if(track->GetDefinition() != G4OpticalPhoton::Definition()) return fUrgent;

    //Only photons born in a fiber core
    const G4VTouchable *touchable = track->GetTouchable();
    if(touchable == nullptr || !G4StrUtil::starts_with(touchable->GetVolume()->GetName(), "FiberCorePhys")) {
        return killOpticalPhotons ? fKill : fUrgent;
    }

    //Fiber identifiers are encoded in the copy number of the core
    G4int copyNumber = touchable->GetCopyNumber();
    G4int det = copyNumber / 100000;
    G4int layer = (copyNumber / 1000) % 100;
    G4int fiber = copyNumber % 1000;

    //Creator process: 0 = Scintillation, 1 = Cerenkov
    const G4VProcess *creator = track->GetCreatorProcess();
    G4int process = (creator != nullptr && creator->GetProcessName() == "Scintillation") ? 0 : 1;

    //Global and local (fiber frame: z along the fiber axis) coordinates
    const G4AffineTransform &toLocal = touchable->GetHistory()->GetTopTransform();
    G4ThreeVector pos = track->GetPosition();
    G4ThreeVector localPos = toLocal.TransformPoint(pos);
    G4ThreeVector localDir = toLocal.TransformAxis(track->GetMomentumDirection());
    G4ThreeVector localPol = toLocal.TransformAxis(track->GetPolarization());
    G4double energy = track->GetKineticEnergy();

    if(storePhotons){
    auto man = G4AnalysisManager::Instance();
    G4int id = 1;
    man->FillNtupleIColumn(id, 0, G4EventManager::GetEventManager()->GetConstCurrentEvent()->GetEventID());
    man->FillNtupleIColumn(id, 1, det);
    man->FillNtupleIColumn(id, 2, layer);
    man->FillNtupleIColumn(id, 3, fiber);
    man->FillNtupleIColumn(id, 4, process);
    man->FillNtupleIColumn(id, 5, track->GetTrackID());
    man->FillNtupleIColumn(id, 6, track->GetParentID());
    man->FillNtupleDColumn(id, 7, pos.x()/CLHEP::cm);
    man->FillNtupleDColumn(id, 8, pos.y()/CLHEP::cm);
    man->FillNtupleDColumn(id, 9, pos.z()/CLHEP::cm);
    man->FillNtupleDColumn(id, 10, localPos.x()/CLHEP::cm);
    man->FillNtupleDColumn(id, 11, localPos.y()/CLHEP::cm);
    man->FillNtupleDColumn(id, 12, localPos.z()/CLHEP::cm);
    man->FillNtupleDColumn(id, 13, localDir.x());
    man->FillNtupleDColumn(id, 14, localDir.y());
    man->FillNtupleDColumn(id, 15, localDir.z());
    man->FillNtupleDColumn(id, 16, localPol.x());
    man->FillNtupleDColumn(id, 17, localPol.y());
    man->FillNtupleDColumn(id, 18, localPol.z());
    man->FillNtupleDColumn(id, 19, energy/CLHEP::eV);
    man->FillNtupleDColumn(id, 20, CLHEP::h_Planck*CLHEP::c_light/energy/CLHEP::nm);
    man->FillNtupleDColumn(id, 21, track->GetGlobalTime()/CLHEP::ns);
    man->AddNtupleRow(id);
    }

    return killOpticalPhotons ? fKill : fUrgent;
}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//
