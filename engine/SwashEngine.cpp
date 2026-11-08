// SwashEngine.cpp - netlist del 4ms Standard Swash (ricostruita dal layout veroboard)
// + aggiunte CV del Noise Swash Eurorack. Vedi docs/standard_swash.cir
#include "SwashEngine.h"
#include <cstdio>

namespace risacca {

namespace {
constexpr double kOn = 10.0;      // interruttore chiuso (0.1 ohm)
constexpr double kOff = 1e-10;    // interruttore aperto
constexpr double LED_IS = 1e-18, LED_N = 2.0;
constexpr double D914_IS = 2.52e-9, D914_N = 1.752;
// potenziometro come reostato: pista (valore totale con la sua tolleranza) + resistenza residua
// agli estremi (al piu' ~20 ohm nei potenziometri a carbone da 16 mm) + contatto del cursore
inline double potR(double total, double pos) { return std::max(total * pos, 0.0) + std::min(20.0, 2e-4 * total) + 1.0; }
}

SwashEngine::SwashEngine() {
    core[0] = std::make_unique<Core>();
    core[1] = std::make_unique<Core>();
}

// ---------------------------------------------------------------------------------
void SwashEngine::buildCore(Core& V, bool euro) {
    Circuit& c = V.c;
    c = Circuit();
    V.euro = euro;
    V.fV9 = c.fixedNode("V9", 0.0);
    V.fVin = c.fixedNode("VINSRC", 0.0);
    V.fVmood = c.fixedNode("VMOOD", 0.0);
    V.fLbo = c.fixedNode("LBOSRC", 0.0);
    V.fU3n = c.fixedNode("U3NSRC", 4.5);
    V.fCl2 = c.fixedNode("CL2SRC", 0.0);
    // le polarizzazioni prese dalla linea Low (R2, R7, R16) vedono VLOW con un campione di ritardo:
    // rompe l'anello algebrico alimentazione -> polarizzazione -> carico (ritardo ~5 us)
    V.fVlowB = c.fixedNode("VLOWB", 0.0);
    const int g = GND, V9 = V.fV9;

    // nodi attivi: linea Low e uscite degli op-amp
    int VLOW = c.node("VLOW", true);
    int U1O = c.node("U1O", true), UBO = c.node("UBO", true), U2O = c.node("U2O", true), U2BO = c.node("U2BO", true);
    // nodi passivi (i nodi intermedi di R-C serie, R-R serie e interruttori sono eliminati)
    int IN = c.node("IN"), U1P = c.node("U1P"), U1N = c.node("U1N"), UBP = c.node("UBP"), UBN = c.node("UBN");
    int A = c.node("A"), LFOJ = c.node("LFOJ"), O = c.node("O"), U2N = c.node("U2N"), C10N = c.node("C10N");
    int UBN2 = c.node("UBN2"), N = c.node("N"), M = c.node("M"), W = c.node("W"), BIAS2 = c.node("BIAS2");

    // --- alimentazione "Low" (1k, pin 2-3) ------------------------------------------
    V.rLow = c.addG(V9, VLOW, 1e-3);
    c.addR(VLOW, g, 1e8);
    // capacita' parassita della linea "Low": regolarizza l'anello alimentazione/carico
    V.cLow = c.addC(VLOW, g, 4.7e-9, "Clow");
    // --- sorgente d'ingresso (Sw5): pickup (R + L) o linea, cavo ------------------------
    V.lPickup = c.addSRL(V.fVin, IN, 7000.0, 2.5);
    V.cCable = c.addC(IN, g, 470e-12, "Ccable");
    c.addR(IN, g, 1e6);                                   // R19
    V.cC1 = c.addSRC(IN, U1P, 10e3, 1e-6, "C1R1");        // C1 + R1
    V.rR2 = c.addR(V.fVlowB, U1P, 220e3);                 // R2
    V.rR3 = c.addR(U1P, g, 220e3);                        // R3
    c.addSRC(IN, V.fU3n, 47e3, 1e-6, "C1u_f");            // carico del ramo clean (massa virtuale 4558)
    // --- U1A pregain --------------------------------------------------------------
    c.addOpAmp(U1P, U1N, U1O, VLOW, modelTL062(), "U1A");
    V.cC2 = c.addSRC(U1N, g, 1e3, 10e-6, "C2R4");         // R4 + C2 (Tame/Trouble)
    V.rPre = c.addR(U1O, U1N, 1.1e5);                     // R5 + PreGain
    // --- U1B comparatore ---------------------------------------------------------
    c.addR(U1O, UBN, 10e3);                               // R6
    c.addC(U1O, UBN, 1e-6, "C3");
    c.addR(V.fVlowB, UBP, 470e3);                         // R7
    V.rGate = c.addR(UBP, g, 5e3);
    V.rSwash = c.addR(UBN, UBP, 5e3);
    // preclip: diodi direttamente tra ingresso - e uscita (Sw4 chiuso), spenti se aperto
    V.dD1 = c.addD(UBN, UBO, D914_IS, D914_N);            // D1
    V.dD2 = c.addD(UBO, UBN, D914_IS, D914_N);            // D2
    V.dD2led = c.addD(UBO, UBN, 1e-40, LED_N);            // D2 come LED rosso (mod)
    c.addOpAmp(UBP, UBN, UBO, VLOW, modelTL062(), "U1B");
    c.addC(UBO, A, 1e-6, "C4");
    // --- nodo A: iniezione LFO (Sw6 polo B) e Mood CV --------------------------------
    V.rSw6 = c.addG(A, LFOJ, kOff);
    V.rC100 = c.addR(LFOJ, V.fLbo, 100.0);
    V.cC10u = c.addC(LFOJ, V.fLbo, 10e-6, "C10u_c");
    V.nLfoj = LFOJ;
    V.rMood = c.addG(V.fVmood, A, kOff);
    // --- tone ------------------------------------------------------------------------------
    if (!euro) {
        int X = c.node("X"), Y = c.node("Y"), B3 = c.node("B3"), B2 = c.node("B2"), B1 = c.node("B1");
        c.addC(A, X, 10e-9, "C10n_b");
        c.addR(X, O, 10e3);
        V.rTopT = c.addR(X, O, 5e4);
        V.rBotT = c.addR(O, Y, 5e4);
        c.addR(O, Y, 1e3);
        c.addC(Y, g, 100e-9, "C100n_c");
        c.addR(A, B3, 4.7e3);
        c.addC(B3, B2, 33e-9, "C33n");
        V.rTopB = c.addR(B3, B2, 5e4);
        c.addR(B2, O, 1e3);
        c.addC(B2, B1, 100e-9, "C100n_d");
        V.rBotB = c.addR(B2, B1, 5e4);
        c.addR(B1, g, 100.0);
        c.addSRC(O, U2N, 10e3, 100e-9, "C9R13");          // C9 (100n) + R13
    } else {
        int XE = c.node("XE"), CE = c.node("CE");
        c.addC(A, XE, 10e-9, "C7");
        c.addR(XE, g, 4.7e3);                             // R10
        c.addR(A, CE, 4.7e3);                             // R8
        V.cC5 = c.addC(CE, g, 100e-9, "C5");
        V.rTopT = c.addR(XE, O, 5e3);
        V.rBotT = c.addR(O, CE, 5e3);
        c.addSRC(O, U2N, 10e3, 1e-6, "C9R13");            // C9 (1u) + R13
    }
    // --- U2A postgain ------------------------------------------------------------
    c.addR(V.fVlowB, BIAS2, 100e3);                       // R16
    c.addR(BIAS2, g, 100e3);                              // R15
    c.addOpAmp(BIAS2, U2N, U2O, VLOW, modelTL072(), "U2A");
    V.rPost = c.addR(U2O, U2N, 5.1e5);                    // R14 + PostGain
    c.addC(U2O, C10N, 100e-9, "C10");
    V.dD3 = c.addD(U2N, C10N, D914_IS, D914_N);           // D3 (Sw3 chiuso)
    V.dD4 = c.addD(C10N, U2N, D914_IS, D914_N);           // D4
    // --- U2B noise vol --------------------------------------------------------------
    c.addSRC(U2O, UBN2, 1e3, 1e-6, "C11R17");             // C11 + R17
    c.addOpAmp(BIAS2, UBN2, U2BO, VLOW, modelTL072(), "U2B");
    V.rNoise = c.addR(U2BO, UBN2, 5e3);
    c.addC(U2BO, UBN2, 330e-12, "C330p");
    c.addSRC(U2BO, N, 1e3, 1e-6, "C12R");                 // C12 + 1k
    c.addR(N, g, 1e6);
    c.addR(N, M, 47e3);
    // --- self-osc (Sw1 + SelfOsc 1M, in serie) -------------------------------------------
    V.rSO = c.addG(N, IN, kOff);
    V.rSO2 = -1;
    // --- miscelazione clean (dal sotto-circuito clean) e master ------------------------
    c.addR(V.fCl2, M, 47e3);
    V.rMT = c.addR(M, W, 5e4);
    V.rMB = c.addR(W, g, 5e4);
    c.addR(W, g, 1e6 + 1e3);                              // 1k + ingresso dell'ampli (1M)
    // --- vactrol Swash CV (Eurorack): LDR1 in serie al pin3 virtuale dello Swash ------------
    V.rLdr1 = c.addG(O, UBP, 1e-7);
    V.rLdr2 = c.addG(UBN, g, 1e-7);

    V.nOut = W; V.nA = A; V.nVlow = VLOW; V.nIN = IN; V.nM = M; V.nU1P = U1P; V.nSO = N;
    c.finalize(fs);
}

void SwashEngine::buildLfo(Lfo& L) {
    Circuit& c = L.c;
    c = Circuit();
    L.fV9 = c.fixedNode("V9", 0.0);
    L.fA = c.fixedNode("ASRC", 0.0);
    const int g = GND;
    int VCCL = c.node("VCCL", true), LAO = c.node("LAO", true), LBO = c.node("LBO", true);
    int LP = c.node("LP"), LN = c.node("LN"), SPD = c.node("SPD"), S = c.node("S"), LEDM = c.node("LEDM");
    L.rSwPow = c.addG(L.fV9, VCCL, kOff);    // Sw6 polo A
    c.addR(VCCL, g, 1e8);
    c.addR(VCCL, LP, 470e3);
    c.addR(LP, g, 470e3);
    c.addR(LAO, LP, 100e3);
    c.addC(LAO, LN, 1e-9, "C1n");
    c.addR(LN, S, 220e3);
    c.addOpAmp(LP, LN, LAO, VCCL, model4558(), "LA");
    c.addR(LAO, SPD, 100.0);
    L.rSpeed = c.addR(SPD, S, 5e4);
    c.addC(S, g, 10e-6, "C10u_b");
    c.addOpAmp(S, LBO, LBO, VCCL, model4558(), "LB");
    c.addR(LBO, LEDM, 1e3);
    L.dLed = c.addD(LBO, LEDM, LED_IS, LED_N);
    c.addR(LEDM, g, 1e3);
    // carico verso il nodo A del core (Sw6 polo B): corrente calcolata nel core
    // (accoppiamento in corrente: stabile perche' l'inseguitore ha impedenza d'uscita quasi nulla)

    L.iLoad = c.addI(LBO);
    L.nLbo = LBO;
    c.finalize(fs);
}

void SwashEngine::buildClean(Clean& K) {
    Circuit& c = K.c;
    c = Circuit();
    K.fV9 = c.fixedNode("V9", 0.0);
    K.fIn = c.fixedNode("INSRC", 0.0);
    K.fM = c.fixedNode("MSRC", 0.0);
    const int g = GND;
    int U3O = c.node("U3O", true);
    int CL1 = c.node("CL1"), U3N = c.node("U3N"), BIAS3 = c.node("BIAS3"), CL2 = c.node("CL2");
    c.addC(K.fIn, CL1, 1e-6, "C1u_f");
    c.addR(CL1, U3N, 47e3);
    c.addR(BIAS3, g, 220e3);
    c.addR(K.fV9, BIAS3, 220e3);
    c.addOpAmp(BIAS3, U3N, U3O, K.fV9, model4558(), "U3");
    K.rClean = c.addR(U3O, U3N, 1.0);
    c.addC(U3O, CL2, 1e-6, "C1u_g");
    c.addR(CL2, K.fM, 47e3);
    K.nU3n = U3N; K.nCl2 = CL2;
    c.finalize(fs);
}

// ---------------------------------------------------------------------------------
void SwashEngine::prepare(double internalSampleRate, uint32_t s) {
    fs = internalSampleRate;
    buildCore(*core[0], false);
    buildCore(*core[1], true);
    buildLfo(lfo);
    buildClean(cln);
    setSeed(s);
    firstControls = true;
    reset();
}

uint64_t SwashEngine::nonConverged() const {
    return core[0]->c.nonConverged + core[1]->c.nonConverged + lfo.c.nonConverged + cln.c.nonConverged;
}

// tolleranze: resistenze fisse 1%, condensatori 5%, offset degli op-amp
void SwashEngine::seedCircuit(Circuit& c, const Circuit& nom, int bank) {
    for (size_t k = 0; k < c.res.size(); ++k) {
        double t = 1.0 + 0.01 * std::clamp(grand(), -2.5, 2.5);
        c.setG((int)k, nom.res[k].g / t);
    }
    // condensatori: elettrolitici (da 1 uF in su) +-20% di targa, film e ceramici +-10%
    for (size_t k = 0; k < c.caps.size(); ++k) {
        const double sig = nom.caps[k].c >= 0.99e-6 ? 0.08 : 0.04;
        double t = 1.0 + sig * std::clamp(grand(), -2.5, 2.5);
        c.setC((int)k, nom.caps[k].c * t);
    }
    for (size_t k = 0; k < c.srcs.size(); ++k) {
        const double sig = nom.srcs[k].c >= 0.99e-6 ? 0.08 : 0.04;
        double t = 1.0 + sig * std::clamp(grand(), -2.5, 2.5);
        c.setSRC_C((int)k, nom.srcs[k].c * t);
    }
    // op-amp: offset, e dispersione tra esemplari di banda, slew rate e corrente di riposo
    for (size_t k = 0; k < c.opamps.size() && k < 16; ++k) {
        auto& op = c.opamps[k];
        const auto& m0 = nom.opamps[k].m;
        op.vos = op.m.vosSigma * std::clamp(grand(), -2.5, 2.5);
        op.m.gbw  = m0.gbw  * (1.0 + 0.10 * std::clamp(grand(), -2.0, 2.0));
        op.m.slew = m0.slew * (1.0 + 0.10 * std::clamp(grand(), -2.0, 2.0));
        op.m.iq   = m0.iq   * (1.0 + 0.10 * std::clamp(grand(), -2.0, 2.0));
        noiseScale[bank][k] = op.m.noise * std::sqrt(0.5 * fs);
    }
}

void SwashEngine::setSeed(uint32_t s) {
    seed = s;
    rng = seed * 2654435761u + 12345u; if (rng == 0) rng = 1;
    for (int k = 0; k < 8; ++k) urand();
    { Core t; buildCore(t, false); seedCircuit(core[0]->c, t.c, 0); }
    // C2 e C5 vengono riscritti dai comandi (Tame/trouble, mod del tono): si conserva la tolleranza
    core[0]->kC2 = core[0]->c.srcs[core[0]->cC2].c / 10e-6;
    uint32_t keep = rng;
    rng = seed * 2654435761u + 12345u; if (rng == 0) rng = 1;   // stessa sequenza: stesso "esemplare"
    for (int k = 0; k < 8; ++k) urand();
    { Core t; buildCore(t, true); seedCircuit(core[1]->c, t.c, 0); }
    core[1]->kC2 = core[1]->c.srcs[core[1]->cC2].c / 10e-6;
    core[1]->kC5 = core[1]->c.caps[core[1]->cC5].c / 100e-9;
    rng = keep;
    { Lfo t; buildLfo(t); seedCircuit(lfo.c, t.c, 1); }
    { Clean t; buildClean(t); seedCircuit(cln.c, t.c, 2); }
    // potenziometri a carbone: valore totale +-20% di targa
    for (int k = 0; k < kPots; ++k) potK[k] = 1.0 + 0.07 * std::clamp(grand(), -2.8, 2.8);
    rng = seed ^ 0x9E3779B9u; if (rng == 0) rng = 1;
}

void SwashEngine::reset() {
    core[0]->c.resetState();
    core[1]->c.resetState();
    lfo.c.resetState();
    cln.c.resetState();
    v9 = 0.0;
    ldrG1 = ldrG2 = 1e-7;
}

void SwashEngine::copyState(const Core& from, Core& to) {
    for (size_t i = 0; i < to.c.names.size(); ++i) {
        to.c.x[i] = 0.0;
        for (size_t j = 0; j < from.c.names.size(); ++j)
            if (from.c.names[j] == to.c.names[i]) { to.c.x[i] = from.c.x[j]; break; }
    }
    for (size_t i = 0; i < to.c.caps.size(); ++i) {
        to.c.caps[i].v = 0.0; to.c.caps[i].i = 0.0;
        for (size_t j = 0; j < from.c.caps.size(); ++j)
            if (from.c.capNames[j] == to.c.capNames[i]) { to.c.caps[i].v = from.c.caps[j].v; break; }
    }
    for (size_t i = 0; i < to.c.inds.size() && i < from.c.inds.size(); ++i) to.c.inds[i] = from.c.inds[i];
    for (size_t i = 0; i < to.c.srls.size() && i < from.c.srls.size(); ++i) { to.c.srls[i].v = from.c.srls[i].v; to.c.srls[i].i = from.c.srls[i].i; }
    for (size_t i = 0; i < to.c.srcs.size(); ++i) {
        to.c.srcs[i].u = 0.0; to.c.srcs[i].i = 0.0;
        for (size_t j = 0; j < from.c.srcs.size(); ++j)
            if (from.c.srcNames[j] == to.c.srcNames[i]) { to.c.srcs[i].u = from.c.srcs[j].u; break; }
    }
    for (size_t i = 0; i < to.c.opamps.size() && i < from.c.opamps.size(); ++i)
        to.c.opamps[i].vi = from.c.opamps[i].vi;
}

void SwashEngine::applyControls(Core& V, const SwashControls& s, double dtSec) {
    Circuit& c = V.c;
    auto setR = [&](int idx, double ohms) { c.setG(idx, 1.0 / ohms); };
    auto sw = [&](int idx, bool on) { c.setG(idx, on ? kOn : kOff); };

    bool powered = s.engaged || !s.tame;
    c.setG(V.rLow, powered ? 1.0 / ((1.0 - s.low) * 1000.0 * potK[kLow] + 3.3) : kOff);
    // sorgente: chitarra (7k + 2.5H, cavo 470p), linea (600 ohm, cavo 100p) o niente collegato
    if (!s.engaged || s.source == 2) { c.setSRL(V.lPickup, 1e12, 1e-5); c.setC(V.cCable, s.source == 2 ? 20e-12 : 470e-12); }
    else if (s.source == 1) { c.setSRL(V.lPickup, 7000.0, 2.5); c.setC(V.cCable, 470e-12); }
    else { c.setSRL(V.lPickup, 600.0, 1e-5); c.setC(V.cCable, 100e-12); }
    setR(V.rPre, 10e3 + potR(1e6 * potK[kPre], s.pregain));
    setR(V.rGate, potR(10e3 * potK[kGate], s.gate));
    setR(V.rSwash, potR(10e3 * potK[kSwash], s.swash));
    // preclip / postclip: i diodi esistono solo con l'interruttore chiuso
    const double isD2 = s.modLedPreclip ? 1e-40 : D914_IS, isL = s.modLedPreclip ? LED_IS : 1e-40;
    c.diodes[V.dD1].is = s.preclip ? D914_IS : 1e-40;
    c.diodes[V.dD2].is = s.preclip ? isD2 : 1e-40;
    c.diodes[V.dD2led].is = s.preclip ? isL : 1e-40;
    c.diodes[V.dD3].is = s.postclip ? D914_IS : 1e-40;
    c.diodes[V.dD4].is = s.postclip ? D914_IS : 1e-40;
    sw(V.rSw6, s.lfoOn);
    c.setG(V.rMood, s.moodPatched ? 1.0 / 1000.0 : kOff);
    if (!V.euro) {
        setR(V.rTopT, potR(100e3 * potK[kTreble], 1.0 - s.treble));
        setR(V.rBotT, potR(100e3 * potK[kTreble], s.treble));
        setR(V.rTopB, potR(100e3 * potK[kBass], 1.0 - s.bass));
        setR(V.rBotB, potR(100e3 * potK[kBass], s.bass));
    } else {
        setR(V.rTopT, potR(10e3 * potK[kTreble], 1.0 - s.treble));
        setR(V.rBotT, potR(10e3 * potK[kTreble], s.treble));
        c.setC(V.cC5, ((s.modTone == 2) ? 10e-9 : 100e-9) * V.kC5);
    }
    setR(V.rPost, 10e3 + potR(1e6 * potK[kPost], s.postgain));
    setR(V.rNoise, potR(10e3 * potK[kNoise], s.noiseVol));
    c.setG(V.rSO, s.selfOscOn ? 1.0 / potR(1e6 * potK[kSO], s.selfOsc) : kOff);
    setR(V.rMT, potR(100e3 * potK[kMaster], 1.0 - s.master));
    setR(V.rMB, potR(100e3 * potK[kMaster], s.master));
    if (clowOverride > 0) c.setC(V.cLow, clowOverride);
    // C2: Tame = 10n, Trouble = 10u (Sw2 polo B)
    c.setSRC_C(V.cC2, (s.tame ? 10e-9 : 10e-6) * V.kC2);

    // vactrol: U3A TL072 (-100k/15k) su +-15V, 220 ohm + trimmer 5k
    double vo = std::min(13.5, 6.6667 * std::fabs(s.swashCV));
    double iLed = std::max(0.0, vo - 1.9) / (220.0 + 5000.0);
    double gt = 1e-7 + std::pow(iLed / 1e-3, 0.9) / 4000.0;
    double t1 = (s.swashCV > 0.0) ? gt : 1e-7;
    double t2 = (s.swashCV < 0.0) ? gt : 1e-7;
    auto lag = [&](double& gcur, double tgt) {
        double tau = (tgt > gcur) ? 0.004 : 0.060;
        gcur += (tgt - gcur) * std::min(1.0, dtSec / tau);
    };
    lag(ldrG1, t1); lag(ldrG2, t2);
    c.setG(V.rLdr1, 1.0 / (1.0 / ldrG1 + potR(10e3 * potK[kSwash], 1.0 - s.swash)));   // LDR1 + pin3-cursore dello Swash
    c.setG(V.rLdr2, ldrG2);

    // LFO e clean
    lfo.c.setG(lfo.rSwPow, s.lfoOn ? kOn : kOff);
    lfo.c.setG(lfo.rSpeed, 1.0 / potR(100e3 * potK[kSpeed], 1.0 - s.lfoSpeed));
    cln.c.setG(cln.rClean, 1.0 / potR(1e6 * potK[kClean], s.clean));
}

void SwashEngine::smoothControls(double dtSec) {
    if (firstControls) { smooth = target; firstControls = false; }
    const double k = std::min(1.0, dtSec / 0.010);   // smoothing manopole ~10 ms
    auto sm = [&](double& a, double b) { a += (b - a) * k; };
    sm(smooth.gate, target.gate); sm(smooth.swash, target.swash); sm(smooth.bass, target.bass);
    sm(smooth.clean, target.clean); sm(smooth.pregain, target.pregain); sm(smooth.treble, target.treble);
    sm(smooth.lfoSpeed, target.lfoSpeed); sm(smooth.selfOsc, target.selfOsc); sm(smooth.low, target.low);
    sm(smooth.master, target.master); sm(smooth.postgain, target.postgain); sm(smooth.noiseVol, target.noiseVol);
    sm(smooth.moodV, target.moodV); sm(smooth.swashCV, target.swashCV);
    smooth.preclip = target.preclip; smooth.postclip = target.postclip; smooth.selfOscOn = target.selfOscOn;
    smooth.tame = target.tame; smooth.lfoOn = target.lfoOn; smooth.engaged = target.engaged;
    smooth.moodPatched = target.moodPatched; smooth.modLedPreclip = target.modLedPreclip;
    smooth.modTone = target.modTone; smooth.source = target.source;
}

double SwashEngine::stepCircuits(double vin, double dtSec, bool noise, double v9Force) {
    int want = smooth.modTone == 0 ? 0 : 1;
    if (want != cur) { copyState(*core[cur], *core[want]); cur = want; }
    Core& V = *core[cur];
    Circuit& c = V.c;
    applyControls(V, smooth, dtSec);

    if (v9Force >= 0.0) v9 = v9Force;
    else v9 += (9.0 - v9) * std::min(1.0, dtSec / 0.004);

    // LFO: il carico del nodo A e' la corrente calcolata nel core al campione precedente
    lfo.c.fixedV[fixedIndex(lfo.fV9)] = v9;
    {
        double iR = c.res[V.rC100].g * (c.v(V.nLfoj) - c.v(V.fLbo));
        double iC = c.caps[V.cC10u].i;
        lfo.c.setI(lfo.iLoad, iR + iC);
    }
    // l'LFO e' lento: si risolve a 1/lfoDiv della frequenza interna con interpolazione lineare
    if (++lfoCount >= lfoDiv || dtSec * fs > 1.5) {
        lfoCount = 0;
        const double hL = std::max(dtSec, lfoDiv / fs);
        lfo.c.setSampleRate(1.0 / hL);
        for (size_t i = 0; i < lfo.c.opamps.size(); ++i) lfo.c.opamps[i].noiseNow = noise ? noiseScale[1][i] * grand() : 0.0;
        lboPrev = lboNext;
        lfo.c.step();
        lboNext = lfo.c.v(lfo.nLbo);
    }
    const double lboNow = lboPrev + (lboNext - lboPrev) * (lfoCount + 1) / (double)lfoDiv;
    // clean
    cln.c.fixedV[fixedIndex(cln.fV9)] = v9;
    cln.c.fixedV[fixedIndex(cln.fIn)] = c.v(V.nIN);
    cln.c.fixedV[fixedIndex(cln.fM)] = c.v(V.nM);
    for (size_t i = 0; i < cln.c.opamps.size(); ++i) cln.c.opamps[i].noiseNow = noise ? noiseScale[2][i] * grand() : 0.0;
    cln.c.step();
    // core
    c.fixedV[fixedIndex(V.fV9)] = v9;
    c.fixedV[fixedIndex(V.fVlowB)] = c.v(V.nVlow);
    c.fixedV[fixedIndex(V.fVin)] = vin;
    c.fixedV[fixedIndex(V.fVmood)] = smooth.moodV;
    c.fixedV[fixedIndex(V.fLbo)] = lboNow;
    c.fixedV[fixedIndex(V.fU3n)] = cln.c.v(cln.nU3n);
    c.fixedV[fixedIndex(V.fCl2)] = cln.c.v(cln.nCl2);
    for (size_t i = 0; i < c.opamps.size(); ++i) c.opamps[i].noiseNow = noise ? noiseScale[0][i] * grand() : 0.0;
    int itc = c.step();
    itAvg += (itc - itAvg) * 0.0005;
    return c.v(V.nOut);
}

// porta il circuito al punto di lavoro "a regime" (condensatori carichi), come dopo
// qualche secondo dall'accensione del pedale: passi temporali crescenti
void SwashEngine::settle(double seconds) {
    // Transitorio d'accensione accorciato: alimentazione gia' a regime, poi si imposta la carica
    // di C2 (unica costante di tempo di parecchi secondi: 10 uF x ~560 kohm) al valore di continua
    // (in DC il pregain e' a guadagno unitario: V(C2) = V(U1N) = V(U1P)) e si lascia assestare il resto.
    smoothControls(1.0);
    smooth = target;
    Circuit* cs[3] = { &core[cur]->c, &lfo.c, &cln.c };
    for (auto* cc : cs) cc->setSampleRate(fs);
    const int n1 = (int)(0.01 * fs);
    for (int i = 0; i < n1; ++i) stepCircuits(0.0, 1.0 / fs, false, 9.0);
    Core& V = *core[cur];
    // polarizzazione DC di U1A+: partitore R2/R3 dalla linea Low
    const double gR2 = V.c.res[V.rR2].g, gR3 = V.c.res[V.rR3].g;
    const double u1p = V.c.v(V.nVlow) * gR2 / (gR2 + gR3);
    V.c.srcs[V.cC2].u = u1p;  V.c.srcs[V.cC2].i = 0.0;      // C2: V(U1N) = V(U1P)
    V.c.srcs[V.cC1].u = -u1p; V.c.srcs[V.cC1].i = 0.0;      // C1: IN = 0, lato R1 = U1P

    const int n2 = (int)(std::max(0.1, seconds) * fs);
    for (int i = 0; i < n2; ++i) stepCircuits(0.0, 1.0 / fs, false, 9.0);

    for (auto* cc : cs) cc->nonConverged = 0;
    v9 = 9.0;
}

double SwashEngine::process(double vin) {
    const double dtSec = 1.0 / fs;
    smoothControls(dtSec);
    double out = stepCircuits(vin, dtSec, true, -1.0);
    Core& V = *core[cur];
    Circuit& c = V.c;

    // indicatori
    met.ledOn = smooth.engaged ? 1.0 : 0.0;
    met.ledLfo = std::clamp(lfo.c.diodes[lfo.dLed].current / 4e-3, 0.0, 1.0);
    double iMood = std::max(0.0, c.v(V.nA) - 0.65 - 2.7) / 1000.0;
    met.ledMood = std::clamp(iMood / 3e-3, 0.0, 1.0);
    met.ledPreclip = smooth.modLedPreclip ? std::clamp(std::fabs(c.diodes[V.dD2led].current) / 3e-3, 0.0, 1.0) : 0.0;
    met.vLow = c.v(V.nVlow);
    met.ledSwashPos = smooth.swashCV > 0 ? std::clamp(smooth.swashCV / 10.0, 0.0, 1.0) : 0.0;
    met.ledSwashNeg = smooth.swashCV < 0 ? std::clamp(-smooth.swashCV / 10.0, 0.0, 1.0) : 0.0;
    return out;
}

} // namespace risacca
