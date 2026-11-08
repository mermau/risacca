#include <cstdlib>
// render.cpp - banco di prova offline del motore (Linux/macOS)
#include "../engine/SwashEngine.h"
#include <cstdio>
#include <chrono>
#include <string>
#include <vector>
#include <cstring>
using namespace risacca;

static void writeWav(const std::string& path, const std::vector<float>& d, int sr) {
    FILE* f = fopen(path.c_str(), "wb"); if (!f) return;
    int n = (int)d.size(); int bytes = n * 2;
    auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); }; auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(1); w16(1);
    w32(sr); w32(sr * 2); w16(2); w16(16); fwrite("data", 1, 4, f); w32(bytes);
    for (float x : d) { float y = std::max(-1.f, std::min(1.f, x)); int16_t s = (int16_t)(y * 32767); fwrite(&s, 2, 1, f); }
    fclose(f);
}

int main(int argc, char** argv) {
    std::string scen = argc > 1 ? argv[1] : "idle";
    double secs = argc > 2 ? atof(argv[2]) : 2.0;
    int os = argc > 3 ? atoi(argv[3]) : 4;
    double fsBase = 48000.0, fs = fsBase * os;
    SwashEngine e;
    for (int i = 4; i < argc; ++i) if (std::string(argv[i]) == "dso=1") e.delaySelfOsc = true;
    e.prepare(fs, 1);
    for (int k = 0; k < 2; ++k) {
        Circuit& cc = e.circuit(k);
        if (auto* v = std::getenv("R_PRED")) cc.predLimit = std::atof(v);
        if (auto* v = std::getenv("R_FIRST")) cc.firstMaxIter = std::atoi(v);
        if (auto* v = std::getenv("R_TOL")) cc.absTol = std::atof(v);
        if (auto* v = std::getenv("R_MAXIT")) cc.maxIter = std::atoi(v);
        if (auto* v = std::getenv("R_LS")) cc.lsThreshold = std::atof(v);
        if (auto* v = std::getenv("R_BIG")) cc.bigStep = std::atof(v);
        if (auto* v = std::getenv("R_DEPTH")) cc.maxDepth = std::atoi(v);
    }
    SwashControls c;
    double inAmp = 1.0;
    long lfoEdges = 0;   // amp=x scala il segnale di prova
    // parametri da riga di comando key=val
    for (int i = 4; i < argc; ++i) {
        char k[64]; double v; if (sscanf(argv[i], "%63[^=]=%lf", k, &v) != 2) continue;
        std::string key = k;
        if (key == "gate") c.gate = v; else if (key == "swash") c.swash = v; else if (key == "bass") c.bass = v;
        else if (key == "clean") c.clean = v; else if (key == "pregain") c.pregain = v; else if (key == "treble") c.treble = v;
        else if (key == "speed") c.lfoSpeed = v; else if (key == "selfosc") c.selfOsc = v; else if (key == "low") c.low = v;
        else if (key == "master") c.master = v; else if (key == "postgain") c.postgain = v; else if (key == "noise") c.noiseVol = v;
        else if (key == "preclip") c.preclip = v > 0.5; else if (key == "postclip") c.postclip = v > 0.5;
        else if (key == "so") c.selfOscOn = v > 0.5; else if (key == "tame") c.tame = v > 0.5; else if (key == "lfo") c.lfoOn = v > 0.5;
        else if (key == "engaged") c.engaged = v > 0.5; else if (key == "mood") { c.moodPatched = true; c.moodV = v; }
        else if (key == "cv") c.swashCV = v; else if (key == "led") c.modLedPreclip = v > 0.5; else if (key == "tone") c.modTone = (int)v;
        else if (key == "seed") e.setSeed((uint32_t)v);
        else if (key == "amp") inAmp = v;
        else if (key == "src") c.source = (int)v;
        else if (key == "clow") e.clowOverride = v;
        else if (key == "pred") { e.circuit(0).predictor = v > 0.5; e.circuit(1).predictor = v > 0.5; }
        else if (key == "tol") { e.circuit(0).absTol = v; e.circuit(1).absTol = v; }
        else if (key == "rtol") { e.circuit(0).resTol = v; e.circuit(1).resTol = v; }
        else if (key == "maxit") { e.circuit(0).maxIter = (int)v; e.circuit(1).maxIter = (int)v; }
        else if (key == "depth") { e.circuit(0).maxDepth = (int)v; e.circuit(1).maxDepth = (int)v; }
        else if (key == "big") { e.circuit(0).bigStep = v; e.circuit(1).bigStep = v; }
        else if (key == "pit") { e.circuit(0).progressIt = (int)v; e.circuit(1).progressIt = (int)v; }
        else if (key == "pr") { e.circuit(0).progressRatio = v; e.circuit(1).progressRatio = v; }
        else if (key == "plim") { e.circuit(0).predLimit = v; e.circuit(1).predLimit = v; }
        else if (key == "nocm") { for (int k = 0; k < 2; ++k) for (auto& op : e.circuit(k).opamps) op.m.vcmLo = -100.0; }
        else if (key == "glf") { e.circuit(0).glFactor = v; e.circuit(1).glFactor = v; }
        else if (key == "pp") { e.circuit(0).physPredict = v > 0.5; e.circuit(1).physPredict = v > 0.5; }
        else if (key == "sec") { e.circuit(0).secantLimit = v > 0.5; e.circuit(1).secantLimit = v > 0.5; }
        else if (key == "secs") { e.circuit(0).secantSlew = v > 0.5; e.circuit(1).secantSlew = v > 0.5; }
        else if (key == "secc") { e.circuit(0).secantClip = v > 0.5; e.circuit(1).secantClip = v > 0.5; }
        else if (key == "fmi") { e.circuit(0).firstMaxIter = (int)v; e.circuit(1).firstMaxIter = (int)v; }
        else if (key == "ls") { e.circuit(0).lsThreshold = v; e.circuit(1).lsThreshold = v; }
        else if (key == "theta") { e.circuit(0).theta = v; e.circuit(1).theta = v; }
    }
    e.setControls(c);
    bool doSettle = true;
    for (int i = 4; i < argc; ++i) if (std::string(argv[i]) == "settle=0") doSettle = false;
    if (doSettle) e.settle(0.3);
    long N = (long)(secs * fs);
    std::vector<float> out; out.reserve(N / os + 1);
    std::vector<float> vlow; std::vector<float> raw; raw.reserve(N);
    double acc = 0; int cnt = 0;
    long hist[64] = {0};
    auto t0 = std::chrono::steady_clock::now();
    double sumsq = 0, peak = 0;
    for (long i = 0; i < N; ++i) {
        double t = i / fs, vin = 0.0;
        if (scen == "guitar") { // nota pizzicata 110 Hz ogni 1 s
            double tt = fmod(t, 1.0); double ph = fmod(tt * 110.0, 1.0);
            vin = 0.3 * exp(-tt * 3.0) * (2.0 * ph - 1.0) * (tt > 0.002);
        } else if (scen == "sine") vin = 0.2 * sin(2 * M_PI * 220 * t);
        double y = e.process(vin * inAmp);
        { static bool lastOn = false; bool on = e.meters().ledLfo > 0.5; if (on && !lastOn) lfoEdges++; lastOn = on; }
        { int li = e.circuit(0).lastIter; hist[std::min(li, 63)]++; }
        raw.push_back((float)y);
        acc += y; if (++cnt == os) { out.push_back((float)(acc / os / 2.0)); acc = 0; cnt = 0; }
        if (t > 0.3) { sumsq += y * y; peak = std::max(peak, fabs(y)); }
        if (i % (int)(fs / 1000) == 0) vlow.push_back((float)e.meters().vLow);
    }
    auto t1 = std::chrono::steady_clock::now();
    double el = std::chrono::duration<double>(t1 - t0).count();
    double rms = sqrt(sumsq / std::max(1.0, (N - 0.3 * fs)));
    float vmin = 99, vmax = -99; for (size_t i = 300; i < vlow.size(); ++i) { vmin = std::min(vmin, vlow[i]); vmax = std::max(vmax, vlow[i]); }
    printf("%s: rms=%.4f V peak=%.3f V  Vlow[%.2f..%.2f]  iter=%.2f nonconv=%llu  RT=%.2fx\n", scen.c_str(), rms, peak,
           vmin, vmax, e.averageIterations(), (unsigned long long)e.nonConverged(), secs / el);
    printf("lfo: %.2f Hz\n", lfoEdges / secs);
    printf("nonconv core=%llu lfo=%llu clean=%llu\n", (unsigned long long)e.circuit(0).nonConverged, (unsigned long long)e.lfoCircuit().nonConverged, (unsigned long long)e.cleanCircuit().nonConverged);
    printf("hist:"); for (int k = 0; k < 64; ++k) if (hist[k]) printf(" %d:%ld", k, hist[k]); printf("\n");
    if (std::getenv("R_NOWRITE")) return 0;
    writeWav("out_" + scen + ".wav", out, (int)fsBase);
    { FILE* g = fopen(("raw_" + scen + ".f32").c_str(), "wb"); fwrite(raw.data(), 4, raw.size(), g); fclose(g); }
    FILE* f = fopen(("vlow_" + scen + ".txt").c_str(), "w"); for (float v : vlow) fprintf(f, "%f\n", v); fclose(f);
    return 0;
}
