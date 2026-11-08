// Circuit.h - risolutore nodale (MNA) con Newton-Raphson per circuiti audio analogici.
//
// - I nodi "passivi" (solo conduttanze simmetriche) vengono eliminati senza pivoting
//   in un ordine a grado minimo; i nodi "attivi" (uscite degli op-amp, linee di
//   alimentazione) formano un piccolo blocco denso risolto con pivoting parziale.
// - Matrice in formato compresso con tabelle di indici precalcolate.
// - Newton con ricerca lineare sul residuo KCL e suddivisione adattiva del passo.
//
// Parte del progetto RISACCA (Collateral Effects) - emulazione 4ms Noise Swash.
#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <cstring>

namespace risacca {

constexpr int GND = -1;

// Indici negativi <= -2 = nodi a tensione imposta (sorgenti ideali)
inline int fixedIndex(int node) { return -node - 2; }
inline bool isFixed(int node) { return node <= -2; }

struct OpAmpModel {
    double gbw;      // Hz
    double slew;     // V/s
    double avol;     // guadagno DC (V/V)
    double ro;       // ohm resistenza d'uscita
    double ilim;     // A limite di corrente d'uscita
    double iq;       // A corrente di riposo per amplificatore
    double dropHi;   // V distanza minima dell'uscita dal V+
    double dropLo;   // V distanza minima dell'uscita dal V-
    double vOn0;     // V alimentazione sotto cui l'op-amp e' spento
    double vOn1;     // V alimentazione oltre cui l'op-amp e' pienamente attivo
    double vcmLo;    // V modo comune (rispetto a V-) al centro della zona di perdita di guadagno; <-50 = disattivo
    double vcmWidth; // V larghezza della transizione
    double gainLo;   // guadagno residuo vicino al V- (<0 = inversione di fase)
    double noise;    // V/sqrt(Hz) rumore in ingresso
    double vosSigma; // V deviazione standard dell'offset
};

// Modelli tipici (valori di datasheet arrotondati)
inline OpAmpModel modelTL062() {
    return { 1.0e6, 3.5e6, 6.0e3, 180.0, 0.010, 200e-6, 1.5, 1.5, 2.0, 4.0, 1.2, 1.4, 0.1, 42e-9, 3e-3 };
}
inline OpAmpModel modelTL072() {
    return { 3.0e6, 13.0e6, 2.0e5, 75.0, 0.025, 1.4e-3, 1.5, 1.5, 2.5, 5.0, 1.2, 1.4, 0.1, 18e-9, 3e-3 };
}
inline OpAmpModel model4558() {
    return { 3.0e6, 1.0e6, 1.0e5, 75.0, 0.025, 1.75e-3, 1.2, 1.2, 2.0, 4.0, -100.0, 1.0, 1.0, 8e-9, 1e-3 };
}

struct Resistor { int a, b; double g; int saa = -1, sbb = -1, sab = -1, sba = -1; };
struct Capacitor { int a, b; double c; double v = 0.0, i = 0.0; double geq = 0.0, ieq = 0.0;
                   int saa = -1, sbb = -1, sab = -1, sba = -1; };
struct Inductor { int a, b; double l; double v = 0.0, i = 0.0; double geq = 0.0, ieq = 0.0;
                  int saa = -1, sbb = -1, sab = -1, sba = -1; };
// R e C in serie tra a e b (il nodo intermedio e' eliminato analiticamente)
struct SeriesRC { int a, b; double r, c; double u = 0.0, i = 0.0; double geq = 0.0, ieq = 0.0;
                  int saa = -1, sbb = -1, sab = -1, sba = -1; };
// R e L in serie tra a e b
struct SeriesRL { int a, b; double r, l; double v = 0.0, i = 0.0; double geq = 0.0, ieq = 0.0;
                  int saa = -1, sbb = -1, sab = -1, sba = -1; };
struct Diode { int a, b; double is, nvt; double current = 0.0; int saa = -1, sbb = -1, sab = -1, sba = -1; };
struct OpAmp {
    int p, n, o, vcc;              // V- = massa
    int vr = -1000;                // nodo di riferimento per le alimentazioni (default = vcc)
    OpAmpModel m;
    double vi = 0.0;               // stato interno (tensione rispetto al V-)
    double viNew = 0.0;
    double vos = 0.0;              // offset (seed)
    double noiseNow = 0.0;         // campione di rumore corrente
    double iout = 0.0, icc = 0.0;  // ultimi valori (diagnostica)
    int so[4] = { -1, -1, -1, -1 }; // slot riga uscita  x colonne p,n,o,vcc
    int sc[4] = { -1, -1, -1, -1 }; // slot riga vcc     x colonne p,n,o,vcc
};

class Circuit {
public:
    // ---- costruzione -------------------------------------------------------
    int node(const std::string& name, bool active = false) {
        for (size_t k = 0; k < names.size(); ++k)
            if (names[k] == name) { if (active) activeFlag[k] = 1; return (int)k; }
        names.push_back(name); activeFlag.push_back(active ? 1 : 0);
        return (int)names.size() - 1;
    }
    int fixedNode(const std::string& name, double v) {
        for (size_t k = 0; k < fixedNames.size(); ++k)
            if (fixedNames[k] == name) return -(int)k - 2;
        fixedNames.push_back(name); fixedV.push_back(v);
        return -(int)fixedNames.size() - 1;
    }
    int addR(int a, int b, double ohms) { Resistor r; r.a = a; r.b = b; r.g = 1.0 / ohms; res.push_back(r); return (int)res.size() - 1; }
    int addG(int a, int b, double g) { Resistor r; r.a = a; r.b = b; r.g = g; res.push_back(r); return (int)res.size() - 1; }
    int addC(int a, int b, double farad, const std::string& name) {
        Capacitor c; c.a = a; c.b = b; c.c = farad; caps.push_back(c); capNames.push_back(name);
        return (int)caps.size() - 1;
    }
    // sorgente di corrente ideale entrante nel nodo (valore impostato a ogni campione)
    int addI(int node) { isrc.push_back({ node, 0.0 }); return (int)isrc.size() - 1; }
    void setI(int idx, double amps) { isrc[idx].second = amps; }
    int addSRC(int a, int b, double r, double c, const std::string& name) {
        SeriesRC e; e.a = a; e.b = b; e.r = r; e.c = c; srcs.push_back(e); srcNames.push_back(name); return (int)srcs.size() - 1; }
    int addSRL(int a, int b, double r, double l) { SeriesRL e; e.a = a; e.b = b; e.r = r; e.l = l; srls.push_back(e); return (int)srls.size() - 1; }
    void setSRC_R(int k, double r) { if (srcs[k].r != r) { srcs[k].r = r; ++resVersion; } }
    void setSRC_C(int k, double c) { if (srcs[k].c != c) { srcs[k].c = c; ++resVersion; } }
    void setSRL(int k, double r, double l) { if (srls[k].r != r || srls[k].l != l) { srls[k].r = r; srls[k].l = l; ++resVersion; } }
    int addL(int a, int b, double henry) { Inductor l; l.a = a; l.b = b; l.l = henry; inds.push_back(l); return (int)inds.size() - 1; }
    int addD(int anode, int cathode, double is, double n) {
        Diode d; d.a = anode; d.b = cathode; d.is = is; d.nvt = n * 0.025852;
        diodes.push_back(d); return (int)diodes.size() - 1;
    }
    int addOpAmp(int p, int n, int o, int vcc, const OpAmpModel& m, const std::string& name, int railRef = -1000) {
        OpAmp a; a.p = p; a.n = n; a.o = o; a.vcc = vcc; a.m = m; a.vr = (railRef == -1000) ? vcc : railRef;
        opamps.push_back(a); opNames.push_back(name);
        return (int)opamps.size() - 1;
    }

    void finalize(double sampleRate);
    void setSampleRate(double sr) { dt = 1.0 / sr; }

    // ---- simulazione -------------------------------------------------------
    int step();            // un passo temporale; ritorna le iterazioni di Newton
    void resetState();
    void resetPredictor() { xPrev = x; }
    struct Snapshot { std::vector<double> x, cv, ci, lv, li, vi, su, si, rv, ri; };
    void save(Snapshot& s) const {
        s.x = x; s.cv.resize(caps.size()); s.ci.resize(caps.size()); s.lv.resize(inds.size()); s.li.resize(inds.size()); s.vi.resize(opamps.size());
        for (size_t k = 0; k < caps.size(); ++k) { s.cv[k] = caps[k].v; s.ci[k] = caps[k].i; }
        for (size_t k = 0; k < inds.size(); ++k) { s.lv[k] = inds[k].v; s.li[k] = inds[k].i; }
        for (size_t k = 0; k < opamps.size(); ++k) s.vi[k] = opamps[k].vi;
        s.su.resize(srcs.size()); s.si.resize(srcs.size()); s.rv.resize(srls.size()); s.ri.resize(srls.size());
        for (size_t k = 0; k < srcs.size(); ++k) { s.su[k] = srcs[k].u; s.si[k] = srcs[k].i; }
        for (size_t k = 0; k < srls.size(); ++k) { s.rv[k] = srls[k].v; s.ri[k] = srls[k].i; }
    }
    void restore(const Snapshot& s) {
        x = s.x; xPrev = s.x;
        for (size_t k = 0; k < caps.size(); ++k) { caps[k].v = s.cv[k]; caps[k].i = s.ci[k]; }
        for (size_t k = 0; k < inds.size(); ++k) { inds[k].v = s.lv[k]; inds[k].i = s.li[k]; }
        for (size_t k = 0; k < opamps.size(); ++k) opamps[k].vi = s.vi[k];
        for (size_t k = 0; k < srcs.size(); ++k) { srcs[k].u = s.su[k]; srcs[k].i = s.si[k]; }
        for (size_t k = 0; k < srls.size(); ++k) { srls[k].v = s.rv[k]; srls[k].i = s.ri[k]; }
    }

    double v(int nodeIdx) const {
        if (nodeIdx == GND) return 0.0;
        if (isFixed(nodeIdx)) return fixedV[fixedIndex(nodeIdx)];
        return x[nodeIdx];
    }

    std::vector<std::string> names, fixedNames, capNames, opNames;
    std::vector<char> activeFlag;
    std::vector<double> fixedV;
    std::vector<Resistor> res;
    std::vector<Capacitor> caps;
    std::vector<Diode> diodes;
    std::vector<Inductor> inds;
    std::vector<SeriesRC> srcs;
    std::vector<std::string> srcNames;
    std::vector<SeriesRL> srls;
    std::vector<std::pair<int, double>> isrc;
    std::vector<OpAmp> opamps;
    std::vector<double> x;          // tensioni ai nodi

    double theta = 0.55;            // metodo theta (0.5 = trapezi, 1 = Eulero all'indietro)
    int maxIter = 10;
    int maxDepth = 4;               // suddivisioni del passo (fino a 1/16)
    double absTol = 3e-4;
    double resTol = 1e-4;
    double lsThreshold = 0.1;     // sotto questo passo (V) niente ricerca lineare
    int lastIter = 0;
    uint64_t nonConverged = 0;
    double gmin = 1e-12;
    bool debug = false;
    bool predictor = true;
    double glFactor = 30.0;
    bool physPredict = false;
    bool secantLimit = true;
    int firstMaxIter = 10;
    bool secantSlew = false;
    bool secantClip = false;
    double fBest = 1e300;
    std::vector<double> xBest;
    double predLimit = 0.05;
    void setG(int idx, double g) { if (res[idx].g != g) { res[idx].g = g; ++resVersion; } }
    void setC(int idx, double c) { if (caps[idx].c != c) { caps[idx].c = c; ++resVersion; } }
    uint64_t resVersion = 1;
    int depthDbg = 0;
    double bigStep = 100.0;
    int progressIt = 99;
    double progressRatio = 0.3;
    bool allowSplit = true;
    bool jacDone = false;
    int usedDepth = 0, hardDepth = 2, hardEasy = 0;

    void evalOpAmp(OpAmp& op, double vp, double vn, double vo, double vc,
                   double& I, double J[4], double& Icc, double K[4]) const;
    int numNodes() const { return n; }
    int numSlots() const { return nnz; }
    long elimPairs() const { long f = 0; for (size_t k = 0; k + 1 < pStart.size(); ++k) { long m = pStart[k+1]-pStart[k]; f += m*m; } return f; }
    int numActive() const { return (int)act.size(); }

public:
    void debugJacobianCheck();
    bool testSolve(std::vector<double>& xin, int maxIt) { auto keep = x; int mi = maxIter; maxIter = maxIt; x = xin; int it = 0; bool ok = newton(it); xin = x; x = keep; maxIter = mi; return ok; }
private:
    int n = 0, nnz = 0;
    double dt = 1.0 / 192000.0;
    std::vector<int> slot;                    // n*n -> indice valore o -1
    std::vector<int> slotRow, slotCol, diagSlot;
    std::vector<double> val, valBase, b, bBase, valStatic;
    uint64_t staticVersion = 0; double staticDt = -1.0;
    std::vector<int> fixedRes;
    std::vector<double> xNew, dx, xTry, xPrev, rWork;
    // programma di eliminazione
    std::vector<int> pNode, pPP, pStart, pNb, pIP, pPJ, pIJStart, pIJ;
    std::vector<int> act, actSlot;
    std::vector<double> S, sb;
    static constexpr int kMaxDepth = 6;
    std::vector<double> xSave[kMaxDepth + 1];

    int sl(int i, int j) const { return (i >= 0 && j >= 0) ? slot[(size_t)i * n + j] : -1; }
    void buildBase();
    void assembleAt(const std::vector<double>& xv);   // val,b linearizzati in xv
    double residualFromAssembly(const std::vector<double>& xv);
    bool solveLinear();
    bool newton(int& iters);
    void commit();
    void finalOpAmpEval();
    void opAmpPredict();
    void projectBounds(std::vector<double>& xv);
    bool subStep(double h, int depth, int& iters);

    static inline double diodeI(const Diode& d, double vd, double& g) {
        const double xm = 40.0;
        double u = vd / d.nvt;
        if (u > xm) { double e = std::exp(xm); g = d.is * e / d.nvt; return d.is * (e * (1.0 + (u - xm)) - 1.0); }
        double e = std::exp(u); g = d.is * e / d.nvt; return d.is * (e - 1.0);
    }
};

// ---------------------------------------------------------------------------
inline double smoothstep01(double t, double& d) {
    if (t <= 0.0) { d = 0.0; return 0.0; }
    if (t >= 1.0) { d = 0.0; return 1.0; }
    d = 6.0 * t * (1.0 - t);
    return t * t * (3.0 - 2.0 * t);
}

// tanh razionale (frazione continua di Lambert), errore < 2e-7 per |x| < 4.97
inline double fastTanh(double x) {
    if (x > 4.97) return 1.0;
    if (x < -4.97) return -1.0;
    double x2 = x * x;
    double num = x * (135135.0 + x2 * (17325.0 + x2 * (378.0 + x2)));
    double den = 135135.0 + x2 * (62370.0 + x2 * (3150.0 + 28.0 * x2));
    return num / den;
}

// rampa morbida: 0.5(x + sqrt(x^2 + e^2))
inline double softRelu(double x, double e, double& d) {
    double r = std::sqrt(x * x + e * e);
    d = 0.5 * (1.0 + x / r);
    return 0.5 * (x + r);
}

// saturazione con ginocchio netto: x / (1+x^4)^(1/4)
inline double softClip(double x, double& d) {
    double x2 = x * x;
    double q = 1.0 + x2 * x2;
    double r = 1.0 / std::sqrt(std::sqrt(q));
    d = r / q;   // (1+x^4)^(-5/4)
    return x * r;
}

inline void Circuit::evalOpAmp(OpAmp& op, double vp, double vn, double vo, double vc,
                               double& I, double J[4], double& Icc, double K[4]) const {
    const OpAmpModel& m = op.m;
    // J/K indici: 0=vp 1=vn 2=vo 3=vc ; V- = 0
    double dS;
    double s = smoothstep01((vc - m.vOn0) / (m.vOn1 - m.vOn0), dS);
    dS /= (m.vOn1 - m.vOn0);

    // stadio d'ingresso JFET: perdita di guadagno vicino al V- (modo comune)
    double kcm = 1.0, dk = 0.0;
    if (m.vcmLo > -50.0) {
        double vcm = 0.5 * (vp + vn);
        double t = (m.vcmLo - vcm) / m.vcmWidth + 0.5;
        double dSg;
        double Sg = smoothstep01(t, dSg);
        kcm = 1.0 - (1.0 - m.gainLo) * Sg;
        dk = (1.0 - m.gainLo) * dSg / m.vcmWidth;
    }
    double vraw = vp - vn + op.vos + op.noiseNow;
    double vd = kcm * vraw;
    double dgp = kcm + vraw * dk * 0.5;
    double dgn = -kcm + vraw * dk * 0.5;

    // integratore interno (polo dominante) con slew rate, Eulero all'indietro
    double wt = 2.0 * M_PI * m.gbw;
    double th = fastTanh(wt * vd / m.slew);
    double sech2 = 1.0 - th * th;
    double rate = s * m.slew * th;
    double tau = m.avol / wt;
    double den = 1.0 + dt / tau;
    double vi = (op.vi + dt * rate) / den;
    op.viNew = vi;
    double dVi_dVd = dt * s * wt * sech2 / den;
    if (secantSlew && std::fabs(vd) > 1e-12) {       // corda della saturazione di slew
        double sec = dt * rate / vd / den;
        if (sec > dVi_dVd) dVi_dVd = sec;
    }
    double dVi_dVc = dt * m.slew * th * dS / den;

    double vt = vi;                 // stato interno riferito al V- (PSRR elevata in zona lineare)
    double dvt_dvp = dVi_dVd * dgp;
    double dvt_dvn = dVi_dVd * dgn;
    double dvt_dvc = dVi_dVc;

    // stadio d'uscita: saturazione verso le alimentazioni
    double dh;
    double h = 0.02 + softRelu(0.5 * (vc - m.dropHi - m.dropLo), 0.1, dh);
    double dh_dvc = 0.5 * dh;
    double c = 0.5 * (vc - m.dropHi + m.dropLo);
    double y = (vt - c) / h;
    double dsc;
    double sc = softClip(y, dsc);
    double f = c + h * sc, df_dvt = dsc, df_dc = 1.0 - dsc, df_dh = sc - y * dsc;
    if (secantClip && std::fabs(y) > 1e-9) {          // corda della saturazione d'uscita
        double sec = sc / y;
        if (sec > df_dvt) { df_dvt = sec; df_dc = 1.0 - sec; df_dh = sc - y * sec; }
    }
    double df_dvc = df_dvt * dvt_dvc + df_dc * 0.5 + df_dh * dh_dvc;

    double sI = 0.02 + 0.98 * s;
    double dsI = 0.98 * dS;
    const double il = m.ilim;
    const double gl = 1.0 / (glFactor * m.ro);
    double z = (f - vo) / (m.ro * il);
    double tz = fastTanh(z);
    double Iraw = il * tz + gl * (f - vo);
    I = sI * Iraw;
    double dI_df = sI * ((1.0 - tz * tz) / m.ro + gl);
    // oltre il limite di corrente la tangente e' quasi piatta: Newton da quel lato scavalca.
    // Si usa la secante (corda), che coincide con la tangente vicino alla soluzione lineare.
    if (secantLimit) {
        double dv = f - vo;
        if (std::fabs(dv) > 1e-9) {
            double sec = sI * Iraw / dv;
            if (sec > dI_df) dI_df = sec;
        }
    }
    J[0] = dI_df * df_dvt * dvt_dvp;
    J[1] = dI_df * df_dvt * dvt_dvn;
    J[2] = -dI_df;
    J[3] = dI_df * df_dvc + Iraw * dsI;

    // corrente di alimentazione: ramo d'uscita in classe AB, transizione sorgente/assorbimento
    // ampia quanto la corrente di riposo
    const double eps = 0.8 * m.iq;
    double dsp, dq;
    double src = softRelu(I, eps, dsp) - 0.5 * eps;
    double q = smoothstep01(vc / 0.8, dq); dq /= 0.8;
    Icc = q * (s * m.iq + src);
    K[0] = q * dsp * J[0];
    K[1] = q * dsp * J[1];
    K[2] = q * dsp * J[2];
    K[3] = q * (dsp * J[3] + m.iq * dS) + dq * (s * m.iq + src);

    // diodi di protezione dell'uscita verso le alimentazioni: l'uscita non puo' essere
    // trascinata oltre V+ / V- di piu' di una caduta di diodo
    auto pn = [](double u, double& du) { const double xm = 40.0;
        if (u > xm) { double e = std::exp(xm); du = e; return e * (1.0 + (u - xm)) - 1.0; }
        double e = std::exp(u); du = e; return e - 1.0; };
    const double isD = 1e-14, nvtD = 0.026;
    double de1 = 0.0, de2 = 0.0, d1 = -isD, d2 = -isD;
    if (vo - vc > -0.25) d1 = isD * pn((vo - vc) / nvtD, de1);   // uscita -> V+
    if (-vo > -0.25) d2 = isD * pn((-vo) / nvtD, de2);           // massa -> uscita
    double g1 = isD * de1 / nvtD, g2 = isD * de2 / nvtD;
    I += -d1 + d2;
    J[2] += -g1 - g2;
    J[3] += g1;
    Icc -= d1;          // la corrente del diodo superiore entra nella linea V+
    K[2] -= g1;
    K[3] += g1;
    op.iout = I; op.icc = Icc;
}

} // namespace risacca
