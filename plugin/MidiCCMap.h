#pragma once
#include <array>
#include <atomic>
#include <juce_data_structures/juce_data_structures.h>
#include "ParamIDs.h"

// Mappa CC MIDI -> parametro, modificabile con MIDI Learn e salvata nello stato del plugin.
// Lettura/scrittura lock-free: il thread audio la consulta per ogni CC in arrivo.
class MidiCCMap
{
public:
    MidiCCMap() { resetToKeyLabDefaults(); }

    // Mappa di partenza per Arturia KeyLab mkII (modalita' User/MIDI generico):
    // encoder 1-9 = CC 74,71,76,77,93,18,19,16,17 ; fader 1-9 = CC 73,75,79,72,80,81,82,83,85.
    // Se la tua KeyLab invia CC diversi basta usare MIDI Learn (tasto destro sul controllo).
    void resetToKeyLabDefaults()
    {
        for (auto& c : ccToParam) c.store (-1);
        const std::pair<int, int> defaults[] = {
            // encoder: le manopole principali del pedale
            { 74, pid::noisegate }, { 71, pid::swash }, { 76, pid::pregain }, { 77, pid::postgain },
            { 93, pid::low }, { 18, pid::selfOsc }, { 19, pid::noiseVol }, { 16, pid::lfoSpeed },
            { 17, pid::volume },
            // fader: tono, clean e le tensioni CV dell'Eurorack
            { 73, pid::bass }, { 75, pid::treble }, { 79, pid::clean }, { 72, pid::moodCV },
            { 80, pid::swashCV },
        };
        for (auto [cc, p] : defaults) ccToParam[(size_t) cc].store (p);
        ++version;
    }

    void clearAll()
    {
        for (auto& c : ccToParam) c.store (-1);
        ++version;
    }

    int paramForCC (int cc) const { return (cc >= 0 && cc < 128) ? ccToParam[(size_t) cc].load() : -1; }

    int ccForParam (int p) const
    {
        for (int cc = 0; cc < 128; ++cc)
            if (ccToParam[(size_t) cc].load() == p) return cc;
        return -1;
    }

    void assign (int cc, int p)
    {
        if (cc < 0 || cc >= 128) return;
        clearParam (p);
        ccToParam[(size_t) cc].store (p);
        ++version;
    }

    void clearParam (int p)
    {
        for (auto& c : ccToParam)
            if (c.load() == p) c.store (-1);
        ++version;
    }

    static bool isLearnable (int cc) { return cc >= 0 && cc < 120 && cc != 0 && cc != 32; }

    juce::ValueTree toValueTree() const
    {
        juce::ValueTree t ("MIDIMAP");
        for (int cc = 0; cc < 128; ++cc)
        {
            const int p = ccToParam[(size_t) cc].load();
            if (p >= 0 && p < pid::numMappable)
            {
                juce::ValueTree e ("CC");
                e.setProperty ("cc", cc, nullptr);
                e.setProperty ("param", juce::String (pid::all[(size_t) p].id), nullptr);
                t.appendChild (e, nullptr);
            }
        }
        return t;
    }

    void fromValueTree (const juce::ValueTree& t)
    {
        if (! t.isValid()) return;
        for (auto& c : ccToParam) c.store (-1);
        for (auto e : t)
        {
            const int cc = e.getProperty ("cc", -1);
            const auto id = e.getProperty ("param").toString();
            for (int p = 0; p < pid::numMappable; ++p)
                if (id == pid::all[(size_t) p].id && cc >= 0 && cc < 128)
                    ccToParam[(size_t) cc].store (p);
        }
        ++version;
    }

    std::atomic<int> learnParam { -1 }; // parametro in attesa di un CC (-1 = nessuno)
    std::atomic<int> version { 0 };     // incrementato a ogni modifica (per aggiornare la GUI)

private:
    std::array<std::atomic<int>, 128> ccToParam {};
};
