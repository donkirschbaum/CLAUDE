#pragma once
#include "Engine.h"
#include <array>

namespace shaker
{
// Rollen der vier Spuren: Main, Akzent, Textur, Farbe
struct TrackConfig { int steps = 16; int mode = Polymeter; int rate = kRateX1; };

namespace Generator
{
    extern const char* roleNames[kTracks];

    // Erzeugt ein Pattern für eine Rolle. Mit withCfg werden Schritte/Modus/Tempo neu gewählt.
    void genRole (int role, bool withCfg, TrackConfig& cfg, float* pat, float density, float ghost, juce::Random& rng);

    // Verändert einzelne Schritte
    void mutate (float* pat, int steps, float amount, juce::Random& rng);

    // Wählt vier zueinander passende Samples (Helligkeit, Länge, Familie)
    std::array<int, kTracks> pickSamples (const SampleLibrary& lib, juce::Random& rng);

    struct RoleMix { float vol, pan, decMin, decMax; };
    extern const RoleMix roleMix[kTracks];
}
} // namespace shaker
