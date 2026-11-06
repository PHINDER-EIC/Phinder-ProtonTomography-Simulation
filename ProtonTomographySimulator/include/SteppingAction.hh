#ifndef SteppingAction_h
#define SteppingAction_h 1

#include "G4UserSteppingAction.hh"
#include "globals.hh"

class G4OpBoundaryProcess;
class NanowireReadout;

class SteppingAction : public G4UserSteppingAction {

public:
    SteppingAction(NanowireReadout *readout_, G4bool storeExitPhotons_);
    virtual ~SteppingAction();

    virtual void UserSteppingAction(const G4Step *);

private:
    G4OpBoundaryProcess *boundary;
    NanowireReadout *readout;
    G4bool storeExitPhotons;
};

#endif
