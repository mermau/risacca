#pragma once
#include <array>

// Elenco ordinato dei parametri: l'indice e' usato dalla mappa MIDI CC.
// kind: 0 = manopola del pedale (0..1, mostrata 0..10), 1 = interruttore,
//       2 = tensione CV (min..max V), 3 = scelta, 4 = livello in dB
struct ParamInfo
{
    const char* id;
    const char* name;
    int kind;
    float defaultValue;
    float minValue;
    float maxValue;
};

namespace pid
{
inline constexpr std::array<ParamInfo, 26> all {{
    { "noisegate",   "Noisegate",            0, 0.50f,   0.0f,  1.0f },
    { "swash",       "Swash",                0, 0.50f,   0.0f,  1.0f },
    { "bass",        "Bass",                 0, 0.50f,   0.0f,  1.0f },
    { "clean",       "Clean blend",          0, 0.00f,   0.0f,  1.0f },
    { "pregain",     "Pregain",              0, 0.50f,   0.0f,  1.0f },
    { "treble",      "Treble",               0, 0.50f,   0.0f,  1.0f },
    { "lfoSpeed",    "LFO speed",            0, 0.40f,   0.0f,  1.0f },
    { "selfOsc",     "Self-osc",             0, 0.50f,   0.0f,  1.0f },
    { "low",         "Low",                  0, 0.80f,   0.0f,  1.0f },
    { "volume",      "Volume",               0, 0.50f,   0.0f,  1.0f },
    { "postgain",    "Postgain",             0, 0.50f,   0.0f,  1.0f },
    { "noiseVol",    "Noise vol",            0, 0.60f,   0.0f,  1.0f },
    { "preclip",     "Preclip",              1, 0.0f,    0.0f,  1.0f },
    { "postclip",    "Postclip",             1, 0.0f,    0.0f,  1.0f },
    { "selfOscSw",   "Self-osc switch",      1, 0.0f,    0.0f,  1.0f },
    { "tame",        "Tame/Trouble",         1, 0.0f,    0.0f,  1.0f },   // acceso = Tame
    { "lfoSw",       "LFO switch",           1, 0.0f,    0.0f,  1.0f },
    { "engaged",     "Bypass",               1, 1.0f,    0.0f,  1.0f },   // acceso = effetto inserito
    { "moodPatched", "Mood CV patched",      1, 0.0f,    0.0f,  1.0f },
    { "moodCV",      "Mood CV",              2, 0.0f,    0.0f, 15.0f },
    { "swashCV",     "Swash CV",             2, 0.0f,  -15.0f, 15.0f },
    { "modLed",      "Mod: preclip LED",     1, 0.0f,    0.0f,  1.0f },
    { "modTone",     "Mod: tone",            3, 0.0f,    0.0f,  2.0f },
    { "source",      "Input source",         3, 1.0f,    0.0f,  2.0f },
    { "inLevel",     "Input level",          4, 0.0f,  -24.0f, 24.0f },
    { "outLevel",    "Output level",         4, -6.0f, -36.0f, 12.0f },
}};

enum Index
{
    noisegate, swash, bass, clean, pregain, treble, lfoSpeed, selfOsc, low, volume, postgain, noiseVol,
    preclip, postclip, selfOscSw, tame, lfoSw, engaged, moodPatched, moodCV, swashCV,
    modLed, modTone, source, inLevel, outLevel
};

inline constexpr int numMappable = (int) all.size();
inline constexpr const char* qualityId = "quality";   // 0 = 4x, 1 = 8x (non automatizzabile)
inline constexpr const char* seedId = "seed";         // esemplare (tolleranze dei componenti)
} // namespace pid
