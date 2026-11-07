// Circuit.cpp - vedi Circuit.h
#include "Circuit.h"
#include <set>
#include <cstdio>

namespace risacca {

void Circuit::finalize(double sampleRate) {
    dt = 1.0 / sampleRate;
    n = (int)names.size();
    x.assign(n, 0.0); xNew.assign(n, 0.0); dx.assign(n, 0.0); xTry.assign(n, 0.0);
    xPrev.assign(n, 0.0); rWork.assign(n, 0.0); b.assign(n, 0.0); bBase.assign(n, 0.0);
    for (auto& sv : xSave) sv.assign(n, 0.0);

    // --- pattern simmetrico ----------------------------------------------------
    std::vector<std::set<int>> adj(n);
    auto link = [&](int i, int j) {
        if (i < 0 || j < 0 || i == j) return;
        adj[i].insert(j); adj[j].insert(i);
    };
    for (auto& r : res) link(r.a, r.b);
    for (auto& c : caps) link(c.a, c.b);
    for (auto& l : inds) link(l.a, l.b);
    for (auto& e : srcs) link(e.a, e.b);
    for (auto& e : srls) link(e.a, e.b);
    for (auto& d : diodes) link(d.a, d.b);
    for (auto& o : opamps) {
        int t[5] = { o.p, o.n, o.o, o.vcc, o.vr };
        for (int i = 0; i < 5; ++i) for (int j = 0; j < 5; ++j) link(t[i], t[j]);
    }

    // --- ordinamento a grado minimo dei nodi passivi ------------------------------
    std::vector<char> done(n, 0);
    std::vector<std::set<int>> g = adj;
    std::vector<std::pair<int, std::vector<int>>> prog;
    int passiveCount = 0;
    for (int i = 0; i < n; ++i) if (!activeFlag[i]) ++passiveCount;
    for (int stepK = 0; stepK < passiveCount; ++stepK) {
        int best = -1; size_t bestDeg = (size_t)-1;
        for (int i = 0; i < n; ++i) {
            if (done[i] || activeFlag[i]) continue;
            if (g[i].size() < bestDeg) { bestDeg = g[i].size(); best = i; }
        }
        std::vector<int> nb(g[best].begin(), g[best].end());
        prog.push_back({ best, nb });
        for (int u : nb) for (int w : nb) if (u != w) g[u].insert(w);
        for (int u : nb) g[u].erase(best);
        g[best].clear();
        done[best] = 1;
    }
    act.clear();
    for (int i = 0; i < n; ++i) if (activeFlag[i]) act.push_back(i);

    // --- slot compressi (pattern + riempimento) ---------------------------------
    std::set<std::pair<int, int>> pat;
    for (int i = 0; i < n; ++i) { pat.insert({ i, i }); for (int j : adj[i]) pat.insert({ i, j }); }
    for (auto& pv : prog)
        for (int u : pv.second) {
            pat.insert({ pv.first, u }); pat.insert({ u, pv.first });
            for (int w : pv.second) pat.insert({ u, w });
        }
    for (int i : act) for (int j : act) pat.insert({ i, j });
    slot.assign((size_t)n * n, -1);
    slotRow.clear(); slotCol.clear();
    for (auto& pr : pat) {
        slot[(size_t)pr.first * n + pr.second] = (int)slotRow.size();
        slotRow.push_back(pr.first); slotCol.push_back(pr.second);
    }
    nnz = (int)slotRow.size();
    val.assign(nnz, 0.0); valBase.assign(nnz, 0.0);
    diagSlot.assign(n, -1);
    for (int i = 0; i < n; ++i) diagSlot[i] = sl(i, i);

    // --- programma di eliminazione con indici precalcolati ------------------------
    pNode.clear(); pPP.clear(); pStart.clear(); pNb.clear(); pIP.clear(); pPJ.clear(); pIJStart.clear(); pIJ.clear();
    for (auto& pv : prog) {
        int p = pv.first;
        pNode.push_back(p); pPP.push_back(sl(p, p));
        pStart.push_back((int)pNb.size());
        pIJStart.push_back((int)pIJ.size());
        for (int i : pv.second) {
            pNb.push_back(i); pIP.push_back(sl(i, p)); pPJ.push_back(sl(p, i));
            for (int j : pv.second) pIJ.push_back(sl(i, j));
        }
    }
    pStart.push_back((int)pNb.size());
    const int na = (int)act.size();
    actSlot.assign((size_t)na * na, -1);
    for (int r = 0; r < na; ++r) for (int c = 0; c < na; ++c) actSlot[(size_t)r * na + c] = sl(act[r], act[c]);
    S.assign((size_t)na * na, 0.0); sb.assign(na, 0.0);

    // --- slot degli elementi -----------------------------------------------------
    auto setSlots = [&](int a, int bb, int& saa, int& sbb, int& sab, int& sba) {
        saa = sl(a, a); sbb = sl(bb, bb); sab = sl(a, bb); sba = sl(bb, a);
    };
    for (auto& r : res) setSlots(r.a, r.b, r.saa, r.sbb, r.sab, r.sba);
    for (auto& c : caps) setSlots(c.a, c.b, c.saa, c.sbb, c.sab, c.sba);
    for (auto& l : inds) setSlots(l.a, l.b, l.saa, l.sbb, l.sab, l.sba);
    for (auto& e : srcs) setSlots(e.a, e.b, e.saa, e.sbb, e.sab, e.sba);
    for (auto& e : srls) setSlots(e.a, e.b, e.saa, e.sbb, e.sab, e.sba);
    fixedRes.clear();
    for (size_t k = 0; k < res.size(); ++k) if (isFixed(res[k].a) || isFixed(res[k].b)) fixedRes.push_back((int)k);
    valStatic.assign(nnz, 0.0); staticVersion = 0; staticDt = -1.0;
    for (auto& d : diodes) setSlots(d.a, d.b, d.saa, d.sbb, d.sab, d.sba);
    for (auto& o : opamps) {
        int t[4] = { o.p, o.n, o.o, o.vr };
        for (int k = 0; k < 4; ++k) { o.so[k] = sl(o.o, t[k]); o.sc[k] = sl(o.vcc, t[k]); }
    }
}

void Circuit::resetState() {
    std::fill(x.begin(), x.end(), 0.0);
    std::fill(xPrev.begin(), xPrev.end(), 0.0);
    for (auto& c : caps) { c.v = 0.0; c.i = 0.0; }
    for (auto& l : inds) { l.v = 0.0; l.i = 0.0; }
    for (auto& e : srcs) { e.u = 0.0; e.i = 0.0; }
    for (auto& e : srls) { e.v = 0.0; e.i = 0.0; }
    for (auto& d : diodes) d.current = 0.0;
    for (auto& o : opamps) { o.vi = 0.0; o.viNew = 0.0; }
}

// parte lineare (resistenze, condensatori, induttori, sorgenti fisse)
void Circuit::buildBase() {
    auto stampS = [&](int saa, int sbb, int sab, int sba, double gg) {
        if (saa >= 0) valStatic[saa] += gg;
        if (sbb >= 0) valStatic[sbb] += gg;
        if (sab >= 0) { valStatic[sab] -= gg; valStatic[sba] -= gg; }
    };
    if (staticVersion != resVersion || staticDt != dt) {
        std::fill(valStatic.begin(), valStatic.end(), 0.0);
        for (int i = 0; i < n; ++i) valStatic[slot[(size_t)i * n + i]] += gmin;
        for (auto& r : res) stampS(r.saa, r.sbb, r.sab, r.sba, r.g);
        for (auto& c : caps) { c.geq = c.c / (theta * dt); stampS(c.saa, c.sbb, c.sab, c.sba, c.geq); }
        for (auto& l : inds) { l.geq = theta * dt / l.l; stampS(l.saa, l.sbb, l.sab, l.sba, l.geq); }
        for (auto& e : srcs) { e.geq = 1.0 / (e.r + theta * dt / e.c); stampS(e.saa, e.sbb, e.sab, e.sba, e.geq); }
        for (auto& e : srls) { e.geq = theta / (e.l / dt + theta * e.r); stampS(e.saa, e.sbb, e.sab, e.sba, e.geq); }
        staticVersion = resVersion; staticDt = dt;
    }
    std::fill(bBase.begin(), bBase.end(), 0.0);
    for (int k : fixedRes) {
        const auto& r = res[k];
        if (r.a >= 0 && isFixed(r.b)) bBase[r.a] += r.g * fixedV[fixedIndex(r.b)];
        if (r.b >= 0 && isFixed(r.a)) bBase[r.b] += r.g * fixedV[fixedIndex(r.a)];
    }
    for (auto& is : isrc) if (is.first >= 0) bBase[is.first] += is.second;
    auto fixedTerm = [&](int a, int bb, double gg) {
        if (a >= 0 && isFixed(bb)) bBase[a] += gg * fixedV[fixedIndex(bb)];
        if (bb >= 0 && isFixed(a)) bBase[bb] += gg * fixedV[fixedIndex(a)];
    };
    for (auto& c : caps) {
        c.ieq = c.geq * c.v + (1.0 - theta) / theta * c.i;
        if (c.a >= 0) bBase[c.a] += c.ieq;
        if (c.b >= 0) bBase[c.b] -= c.ieq;
        if (c.a < -1 || c.b < -1) fixedTerm(c.a, c.b, c.geq);
    }
    for (auto& e : srcs) {
        // i_n = geq*v_n + ieq ; ieq = -geq*(u_{n-1} + (dt/C)(1-theta) i_{n-1})
        e.ieq = -e.geq * (e.u + dt / e.c * (1.0 - theta) * e.i);
        if (e.a >= 0) bBase[e.a] -= e.ieq;
        if (e.b >= 0) bBase[e.b] += e.ieq;
        if (e.a < -1 || e.b < -1) fixedTerm(e.a, e.b, e.geq);
    }
    for (auto& e : srls) {
        double den = e.l / dt + theta * e.r;
        e.ieq = (e.l / dt * e.i + (1.0 - theta) * (e.v - e.r * e.i)) / den;
        if (e.a >= 0) bBase[e.a] -= e.ieq;
        if (e.b >= 0) bBase[e.b] += e.ieq;
        if (e.a < -1 || e.b < -1) fixedTerm(e.a, e.b, e.geq);
    }
    for (auto& l : inds) {
        // i_n = geq*v_n + ieq, ieq = i_{n-1} + (1-theta) dt/L v_{n-1}
        l.ieq = l.i + (1.0 - theta) * dt / l.l * l.v;
        if (l.a >= 0) bBase[l.a] -= l.ieq;
        if (l.b >= 0) bBase[l.b] += l.ieq;
        if (l.a < -1 || l.b < -1) fixedTerm(l.a, l.b, l.geq);
    }
}

void Circuit::assembleAt(const std::vector<double>& xv) {
    std::memcpy(val.data(), valStatic.data(), sizeof(double) * nnz);
    std::memcpy(b.data(), bBase.data(), sizeof(double) * n);
    auto V = [&](int k) { return k == GND ? 0.0 : (isFixed(k) ? fixedV[fixedIndex(k)] : xv[k]); };
    for (auto& d : diodes) {
        if (d.is < 1e-30) { d.current = 0.0; continue; }   // diodo escluso (interruttore aperto)
        double vd = V(d.a) - V(d.b);
        double gd;
        double id = diodeI(d, vd, gd);
        gd += 1e-12;
        double ieq = id - gd * vd;
        if (d.saa >= 0) val[d.saa] += gd;
        if (d.sbb >= 0) val[d.sbb] += gd;
        if (d.sab >= 0) { val[d.sab] -= gd; val[d.sba] -= gd; }
        if (d.a >= 0) { b[d.a] -= ieq; if (isFixed(d.b)) b[d.a] += gd * V(d.b); }
        if (d.b >= 0) { b[d.b] += ieq; if (isFixed(d.a)) b[d.b] += gd * V(d.a); }
        d.current = id;
    }
    for (auto& op : opamps) {
        int idx[4] = { op.p, op.n, op.o, op.vr };
        double vv[4] = { V(op.p), V(op.n), V(op.o), V(op.vr) };
        double I, J[4], Icc, K[4];
        evalOpAmp(op, vv[0], vv[1], vv[2], vv[3], I, J, Icc, K);
        if (op.o >= 0) {
            double rhs = I;
            for (int k = 0; k < 4; ++k) {
                if (idx[k] >= 0) { rhs -= J[k] * vv[k]; val[op.so[k]] -= J[k]; }
            }
            b[op.o] += rhs;
        }
        if (op.vcc >= 0) {
            double rhs = -Icc;
            for (int k = 0; k < 4; ++k) {
                if (idx[k] >= 0) { rhs += K[k] * vv[k]; val[op.sc[k]] += K[k]; }
            }
            b[op.vcc] += rhs;
        }
    }
}

// residuo KCL nel punto di linearizzazione: F = A x - b
double Circuit::residualFromAssembly(const std::vector<double>& xv) {
    for (int i = 0; i < n; ++i) rWork[i] = -b[i];
    for (int s = 0; s < nnz; ++s) rWork[slotRow[s]] += val[s] * xv[slotCol[s]];
    // residuo scalato per la diagonale (in volt): confronta nodi con conduttanze diverse
    double m = 0.0;
    for (int i = 0; i < n; ++i) {
        double d = std::fabs(val[diagSlot[i]]);
        double r = rWork[i] / (d + 1e-9);
        m += r * r;
    }
    return std::sqrt(m);
}

bool Circuit::solveLinear() {
    const int np = (int)pNode.size();
    for (int k = 0; k < np; ++k) {
        const int p = pNode[k];
        const double pp = val[pPP[k]];
        if (std::fabs(pp) < 1e-300) return false;
        const double inv = 1.0 / pp;
        const int s0 = pStart[k], s1 = pStart[k + 1], m = s1 - s0;
        const int* ij = &pIJ[pIJStart[k]];
        for (int a = 0; a < m; ++a) {
            double f = val[pIP[s0 + a]];
            if (f == 0.0) continue;
            f *= inv;
            const int* row = ij + a * m;
            for (int c = 0; c < m; ++c) val[row[c]] -= f * val[pPJ[s0 + c]];
            b[pNb[s0 + a]] -= f * b[p];
        }
    }
    const int na = (int)act.size();
    for (int r = 0; r < na * na; ++r) S[r] = val[actSlot[r]];
    for (int r = 0; r < na; ++r) sb[r] = b[act[r]];
    for (int k = 0; k < na; ++k) {
        int pr = k; double mx = std::fabs(S[(size_t)k * na + k]);
        for (int r = k + 1; r < na; ++r) {
            double vv = std::fabs(S[(size_t)r * na + k]);
            if (vv > mx) { mx = vv; pr = r; }
        }
        if (mx < 1e-300) return false;
        if (pr != k) {
            for (int c = 0; c < na; ++c) std::swap(S[(size_t)k * na + c], S[(size_t)pr * na + c]);
            std::swap(sb[k], sb[pr]);
        }
        const double inv = 1.0 / S[(size_t)k * na + k];
        for (int r = k + 1; r < na; ++r) {
            double f = S[(size_t)r * na + k] * inv;
            if (f == 0.0) continue;
            for (int c = k + 1; c < na; ++c) S[(size_t)r * na + c] -= f * S[(size_t)k * na + c];
            sb[r] -= f * sb[k];
        }
    }
    for (int k = na - 1; k >= 0; --k) {
        double s = sb[k];
        for (int c = k + 1; c < na; ++c) s -= S[(size_t)k * na + c] * xNew[act[c]];
        xNew[act[k]] = s / S[(size_t)k * na + k];
    }
    for (int k = np - 1; k >= 0; --k) {
        const int p = pNode[k];
        double s = b[p];
        for (int a = pStart[k]; a < pStart[k + 1]; ++a) s -= val[pPJ[a]] * xNew[pNb[a]];
        xNew[p] = s / val[pPP[k]];
    }
    for (int i = 0; i < n; ++i) if (!std::isfinite(xNew[i])) return false;
    return true;
}

bool Circuit::newton(int& iters) {
    buildBase();
    assembleAt(x);
    double f0 = -1.0;   // residuo calcolato solo quando serve la ricerca lineare
    fBest = 1e300;
    int stuck = 0;
    double step0 = 0.0;
    for (int it = 0; it < maxIter; ++it) {
        ++iters;
        if (!solveLinear()) return false;
        double stepMax = 0.0;
        for (int i = 0; i < n; ++i) { dx[i] = xNew[i] - x[i]; stepMax = std::max(stepMax, std::fabs(dx[i])); }
        if (stepMax < absTol) {
            // convergenza: si accetta il passo e si aggiorna lo stato interno degli op-amp
            for (int i = 0; i < n; ++i) x[i] += dx[i];
            finalOpAmpEval();
            return true;
        }
        // passo enorme al primo tentativo: conviene suddividere subito l'intervallo
        if (it <= 1 && stepMax > bigStep && allowSplit) return false;
        if (it == 0) step0 = stepMax;
        // convergenza lenta (non quadratica): suddividere costa meno che insistere
        if (allowSplit && it == progressIt && stepMax > progressRatio * step0) return false;
        double lam = stepMax > 8.0 ? 8.0 / stepMax : 1.0;
        if (stepMax < lsThreshold) {
            for (int i = 0; i < n; ++i) x[i] += dx[i];
            assembleAt(x);
            f0 = -1.0;
            if (debug) { int w = 0; for (int i = 0; i < n; ++i) if (std::fabs(dx[i]) > std::fabs(dx[w])) w = i;
                fprintf(stderr, "  d%d it%d step=%g @%s (no ls)\n", depthDbg, it, stepMax, names[w].c_str()); }
            continue;
        }
        // la soluzione lineare distrugge val/b: per il residuo in x si riassembla (solo passi grandi)
        if (f0 < 0.0) { assembleAt(x); f0 = residualFromAssembly(x); }
        double f1 = 0.0;
        for (int ls = 0; ls < 8; ++ls) {
            for (int i = 0; i < n; ++i) xTry[i] = x[i] + lam * dx[i];
            assembleAt(xTry);
            f1 = residualFromAssembly(xTry);
            if (f1 <= (1.0 - 1e-4 * lam) * f0 || f0 < 1e-13) break;
            lam *= 0.5;
        }
        x.swap(xTry);
        if (f1 < fBest) { fBest = f1; xBest = x; }
        // ricerca lineare bloccata: meglio suddividere subito il passo
        if (lam <= 0.0625) { if (++stuck >= 2) return false; } else stuck = 0;
        if (debug) { int w = 0; for (int i = 0; i < n; ++i) if (std::fabs(dx[i]) > std::fabs(dx[w])) w = i;
            fprintf(stderr, "  d%d it%d step=%g @%s(%.3f) lam=%g res=%g |", depthDbg, it, stepMax, names[w].c_str(), x[w], lam, f1);
            for (auto& op : opamps) fprintf(stderr, " [vo=%.2f vt=%.2f I=%.1fmA]", v(op.o), op.viNew, op.iout * 1e3);
            fprintf(stderr, "\n"); }
        f0 = f1;
        if (debug && depthDbg == 3 && it == 8 && !jacDone) { jacDone = true; debugJacobianCheck(); }
    }
    return false;
}

// le uscite degli op-amp non possono uscire dalle alimentazioni: proiezione del passo di Newton
void Circuit::projectBounds(std::vector<double>& xv) {
    for (auto& op : opamps) {
        if (op.o < 0) continue;
        double vc = op.vr >= 0 ? xv[op.vr] : v(op.vr);
        double hi = std::max(vc, 0.0) + 0.3, lo = -0.3;
        if (xv[op.o] > hi) xv[op.o] = hi;
        if (xv[op.o] < lo) xv[op.o] = lo;
    }
}

void Circuit::opAmpPredict() {
    auto V = [&](int k) { return k == GND ? 0.0 : (isFixed(k) ? fixedV[fixedIndex(k)] : x[k]); };
    for (auto& op : opamps) {
        if (op.o < 0) continue;
        double I, J[4], Icc, K[4];
        // con le tensioni del campione precedente: viNew = stato interno esplicito
        evalOpAmp(op, V(op.p), V(op.n), V(op.o), V(op.vr), I, J, Icc, K);
        double vc = V(op.vr), dh;
        double h = 0.02 + softRelu(0.5 * (vc - op.m.dropHi - op.m.dropLo), 0.1, dh);
        double c = 0.5 * (vc - op.m.dropHi + op.m.dropLo), dsc;
        x[op.o] = c + h * softClip((op.viNew - c) / h, dsc);
    }
}

void Circuit::finalOpAmpEval() {
    auto V = [&](int k) { return k == GND ? 0.0 : (isFixed(k) ? fixedV[fixedIndex(k)] : x[k]); };
    for (auto& op : opamps) {
        double I, J[4], Icc, K[4];
        evalOpAmp(op, V(op.p), V(op.n), V(op.o), V(op.vr), I, J, Icc, K);
    }
    for (auto& d : diodes) { if (d.is < 1e-30) { d.current = 0.0; continue; } double g_; d.current = diodeI(d, V(d.a) - V(d.b), g_); }
}

void Circuit::commit() {
    for (auto& c : caps) {
        double vnow = v(c.a) - v(c.b);
        c.i = c.geq * vnow - c.ieq;
        c.v = vnow;
    }
    for (auto& l : inds) {
        double vnow = v(l.a) - v(l.b);
        l.i = l.geq * vnow + l.ieq;
        l.v = vnow;
    }
    for (auto& e : srcs) {
        double vnow = v(e.a) - v(e.b);
        double inew = e.geq * vnow + e.ieq;
        e.u += dt / e.c * (theta * inew + (1.0 - theta) * e.i);
        e.i = inew;
    }
    for (auto& e : srls) {
        double vnow = v(e.a) - v(e.b);
        e.i = e.geq * vnow + e.ieq;
        e.v = vnow;
    }
    // stato interno degli op-amp: quello dell'ultima linearizzazione (in x),
    // limitato a +-3h attorno al centro di escursione (anti-windup, saturazione quasi completa)
    for (auto& op : opamps) {
        double vc = v(op.vr);
        double dh;
        double h = 0.02 + softRelu(0.5 * (vc - op.m.dropHi - op.m.dropLo), 0.1, dh);
        double c = 0.5 * (vc - op.m.dropHi + op.m.dropLo);
        op.vi = std::clamp(op.viNew, c - 3.0 * h, c + 3.0 * h);
    }
}

bool Circuit::subStep(double h, int depth, int& iters) {
    const double keep = dt;
    dt = h;
    xSave[depth] = x;
    depthDbg = depth;
    allowSplit = depth < maxDepth;
    bool ok = newton(iters);
    if (!ok && depth < maxDepth) {
        x = xSave[depth];
        bool ok1 = subStep(h * 0.5, depth + 1, iters);
        bool ok2 = subStep(h * 0.5, depth + 1, iters);
        dt = keep;
        return ok1 && ok2;
    }
    if (ok && depth > usedDepth) usedDepth = depth;
    if (!ok && fBest < 1e299) { x = xBest; assembleAt(x); }   // miglior iterato trovato
    bool bad = false;
    for (int i = 0; i < n; ++i) if (!std::isfinite(x[i])) bad = true;
    if (bad) { resetState(); dt = keep; return false; }
    commit();
    dt = keep;
    return ok;
}

int Circuit::step() {
    int iters = 0;
    if (predictor) {   // predittore lineare per il punto iniziale
        for (int i = 0; i < n; ++i) {
            double xi = x[i];
            double d = std::clamp(xi - xPrev[i], -predLimit, predLimit);   // estrapolazione limitata
            x[i] = xi + d; xPrev[i] = xi;
        }
    }
    // primo tentativo sul passo intero; se fallisce si passa direttamente alla suddivisione
    // che ha funzionato l'ultima volta (evita i tentativi intermedi destinati a fallire)
    usedDepth = 0;
    bool ok;
    {
        const double keep = dt;
        xSave[0] = x;
        depthDbg = 0;
        allowSplit = maxDepth > 0;
        const int keepIt = maxIter;
        if (maxDepth > 0) maxIter = std::min(maxIter, firstMaxIter);
        ok = newton(iters);
        maxIter = keepIt;
        if (!ok && physPredict) {
            // secondo tentativo: uscite degli op-amp predette dalla loro dinamica interna
            x = xSave[0];
            opAmpPredict();
            ok = newton(iters);
        }
        if (ok || maxDepth == 0) { if (!ok && fBest < 1e299) { x = xBest; assembleAt(x); } commit(); }
        else {
            x = xSave[0];
            const int sd = std::clamp(hardDepth, 1, maxDepth);
            const int m = 1 << sd;
            const double h = keep / m;
            ok = true;
            usedDepth = sd;
            for (int k = 0; k < m; ++k) ok = subStep(h, sd, iters) && ok;
            // la prossima volta: profondita' usata (o una in meno se e' bastata quella di partenza)
            hardDepth = (usedDepth > sd) ? usedDepth : std::max(1, sd - (++hardEasy > 8 ? 1 : 0));
            if (hardEasy > 8) hardEasy = 0;
        }
        dt = keep;
    }
    if (!ok) ++nonConverged;
    lastIter = iters;
    return iters;
}

void Circuit::debugJacobianCheck() {
    buildBase();
    assembleAt(x);
    std::vector<double> A0 = val, b0 = b, F0(n), F1(n);
    auto Fof = [&](std::vector<double>& out) { residualFromAssembly(x); out = rWork; };
    Fof(F0);
    int bad = 0;
    for (int j = 0; j < n; ++j) {
        double h = 1e-7;
        double keep = x[j]; x[j] += h;
        assembleAt(x); Fof(F1);
        x[j] = keep;
        for (int i = 0; i < n; ++i) {
            double num = (F1[i] - F0[i]) / h;
            int s = sl(i, j);
            double an = s >= 0 ? A0[s] : 0.0;
            double sc = std::fabs(an) + std::fabs(num) + 1e-9;
            if (std::fabs(num - an) > 1e-3 * sc + 1e-9) { if (bad++ < 20) fprintf(stderr, "J[%s][%s] an=%g num=%g\n", names[i].c_str(), names[j].c_str(), an, num); }
        }
    }
    assembleAt(x);
    fprintf(stderr, "jac check: %d mismatches\n", bad);
}

} // namespace risacca
