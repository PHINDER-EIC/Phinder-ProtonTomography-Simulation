#ifndef RunAction_h
#define RunAction_h 1

#include "G4UserRunAction.hh"
#include "globals.hh"

class G4Run;
class NanowireReadout;

class RunAction : public G4UserRunAction
{
public:
    RunAction(std::string, NanowireReadout *readout_);
    virtual ~RunAction();

    void BeginOfRunAction(const G4Run*);
    void   EndOfRunAction(const G4Run*);

    std::string nameOfOutputFile;

private:
    NanowireReadout *readout;
};


#endif

