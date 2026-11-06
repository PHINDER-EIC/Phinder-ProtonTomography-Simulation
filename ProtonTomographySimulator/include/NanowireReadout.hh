// NanowireReadout.hh
#ifndef NanowireReadout_h
#define NanowireReadout_h 1
#include <map>
#include <tuple>
#include <vector>

class NanowireReadout {
public:
    NanowireReadout(double dt_ns, double tmax_ns, double tauRise_ns, double tauDecay_ns,
                    double responsivity_nA, double tOffset_ns, int minPhotons);
    void Reset();                                                        // al empezar cada evento
    void AddPhoton(int det, int layer, int fiber, int side, double t_ns);
    void SetReferenceTime(double t_ns) { tRef = t_ns; hasRef = true; }
    void FillNtuple(int ntupleId, int eventNumber);                      // una fila por extremo de fibra
    std::vector<float>& CurrentBuffer() { return current; }              // ligado a la columna vectorial
private:
    double PulseIntegral(double t) const;
    double dt, tmax, tauR, tauD, R, tOffset, peak, tRef = 0;
    bool hasRef = false;
    int nTime, minPhotons;
    std::vector<double> kernel;
    std::map<std::tuple<int,int,int,int>, std::vector<double>> photonTimes; // (det,layer,fiber,side) -> t
    std::vector<float> current;
};
#endif
