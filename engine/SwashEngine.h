// SwashEngine.h - emulazione a livello di circuito del 4ms Standard Swash
// (nucleo comune al Noise Swash Eurorack) con le aggiunte CV dell'Eurorack.
//
// Il circuito e' diviso in tre sotto-circuiti risolti in sequenza ogni campione:
//   core  : alimentazione "Low", TL062 (U1A/U1B), tone, TL072 (U2A/U2B), self-osc, uscita
//   lfo   : 4558 A/B (oscillatore a rilassamento + inseguitore), alimentato da Sw6
//   clean : 4558 (ramo pulito), alimentato dai +9V
// L'accoppiamento tra sotto-circuiti avviene tramite sorgenti con un campione di ritardo
// (a 4x di sovracampionamento: ~5 us, ininfluente in banda audio).
#pragma once

#include "Circuit.h"
#include <memory>

namespace risacca {

struct SwashControls {
    // manopole 0..1 (0 = tutto antiorario)
    double gate = 0.25, swash = 0.45, bass = 0.5, clean = 0.0, pregain = 0.55, treble = 0.5;
    double lfoSpeed = 0.5, selfOsc = 0.4, low = 0.8, master = 0.6, postgain = 0.5, noiseVol = 0.7;
    // interruttori
    bool preclip = false, postclip = false, selfOscOn = true, tame = false, lfoOn = false, engaged = true;
    // CV (aggiunta Eurorack)
    bool moodPatched = false;
    double moodV = 0.0;     // 0..15 V
    double swashCV = 0.0;   // -15..+15 V
    // mod nascoste
    bool modLedPreclip = false;  // D2 = LED rosso (Eurorack)
    int modTone = 0;             // 0 = Standard (Bass/Treble), 1 = Eurorack C5 100n, 2 = Eurorack C5 10n
    int source = 1;              // 0 = linea (600 ohm), 1 = chitarra (pickup + cavo), 2 = nulla collegato
};

struct SwashMeters {
    double ledOn = 0, ledLfo = 0, ledMood = 0, ledPreclip = 0, vLow = 0, ledSwashPos = 0, ledSwashNeg = 0;
};

class SwashEngine {
public:
    SwashEngine();
    void prepare(double internalSampleRate, uint32_t seed);
    void reset();                         // stacca e riattacca l'alimentazione
    void settle(double seconds = 0.3);    // porta il circuito a regime (dopo setControls)
    void setControls(const SwashControls& c) { target = c; }
    void setSeed(uint32_t seed);
    double process(double vin);           // vin in volt -> tensione all'uscita dell'effetto
    const SwashMeters& meters() const { return met; }
    double averageIterations() const { return itAvg; }
    uint64_t nonConverged() const;
    Circuit& circuit(int k) { return core[k]->c; }
    Circuit& lfoCircuit() { return lfo.c; }
    Circuit& cleanCircuit() { return cln.c; }
    double clowOverride = -1.0;
    bool delaySelfOsc = false;
    bool debugSettle = false;   // da impostare prima di prepare()

private:
    struct Core {
        Circuit c;
        int rLow, rSrc, rPre, rGate, rSwash, rSw3pin, rLdr1, rLdr2, rSw4;
        int rTopT, rBotT, rTopB = -1, rBotB = -1;
        int rPost, rSw3, rNoise, rSw1, rSO, rMT, rMB, rSw6, rMood;
        int cC2, cC5 = -1, cCable, lPickup, dD1, dD2, dD2led, dD3, dD4, cLow;
        int nOut, nA, nVlow, nIN, nM, nLfoj, rC100, cC10u, nU1P, rR2, rR3, cC1;
        int fV9, fVin, fVmood, fLbo, fU3n, fCl2, fVlowB, fSOin = 0, fSOso = 0, rSO2 = -1, nSO = 0;
        bool euro = false;
        double kC2 = 1.0, kC5 = 1.0;   // tolleranza di C2 e C5 dell'esemplare
    };
    struct Lfo {
        Circuit c;
        int rSwPow, rSpeed, dLed, iLoad;
        int fV9, fA;
        int nLbo;
    };
    struct Clean {
        Circuit c;
        int rClean;
        int fV9, fIn, fM;
        int nU3n, nCl2;
    };
    void buildCore(Core& v, bool euro);
    void buildLfo(Lfo& l);
    void buildClean(Clean& k);
    void applyControls(Core& v, const SwashControls& s, double dtSec);
    void copyState(const Core& from, Core& to);
    void seedCircuit(Circuit& c, const Circuit& nominal, int noiseBase);
    void smoothControls(double dtSec);
    double stepCircuits(double vin, double dtSec, bool noise, double v9Force);

    std::unique_ptr<Core> core[2];
    Lfo lfo;
    Clean cln;
    int cur = 0;
    double fs = 192000.0;
    SwashControls target, smooth;
    bool firstControls = true;
    double v9 = 0.0;                     // rampa di accensione
    double ldrG1 = 1e-7, ldrG2 = 1e-7;   // conduttanze vactrol
    enum { kLow, kPre, kGate, kSwash, kTreble, kBass, kPost, kNoise, kSO, kMaster, kSpeed, kClean, kPots };
    double potK[kPots] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };   // tolleranza dei potenziometri
    uint32_t seed = 1, rng = 1;
    double noiseScale[3][16] {};
    SwashMeters met;
    double itAvg = 0.0;
    int lfoDiv = 4, lfoCount = 0;
    double lboPrev = 0.0, lboNext = 0.0;

    inline double urand() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (rng >> 8) * (1.0 / 16777216.0); }
    inline double grand() { return (urand() + urand() + urand() + urand() - 2.0) * 1.7320508; }
};

} // namespace risacca
