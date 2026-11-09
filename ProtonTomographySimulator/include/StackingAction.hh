#ifndef StackingAction_h
#define StackingAction_h 1

#include "G4UserStackingAction.hh"
#include "globals.hh"

class StackingAction : public G4UserStackingAction {

public:
    StackingAction(G4bool kill, G4bool storePhotons_);
    virtual ~StackingAction();

    virtual G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track *);

private:
    G4bool killOpticalPhotons;
    G4bool storePhotons;
};

#endif
