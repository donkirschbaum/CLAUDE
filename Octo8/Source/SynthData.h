#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <vector>
#include <cmath>

namespace octo
{
constexpr int NCH = 8;
constexpr int NSTEPS = 16;
constexpr int NWAVES = 21;
constexpr int SUPERSAW = 20;
constexpr int SAW = 3;
constexpr int NNOISE = 9;

inline const juce::StringArray& waveNames()
{
    static const juce::StringArray a { "Sine", "Triangle", "Square", "Sawtooth", "Reverse Saw",
        "Pulse 25%", "Pulse 12.5%", "Pulse 5%", "Half-Sine", "Rectified Sine", "Trapezoid",
        "Stepped 4-bit", "Parabolic", "Sine Fold", "Sync Saw", "Organ", "Odd Harmonics",
        "Bell", "Vocal Ah", "Brass", "Supersaw" };
    return a;
}

inline const juce::StringArray& noiseNames()
{
    static const juce::StringArray a { "White Noise", "Pink Noise", "Brown Noise", "Blue Noise",
        "Violet Noise", "Grey Noise", "Velvet Noise", "Crackle Noise", "Digital S&H Noise" };
    return a;
}

inline const juce::StringArray& noteNames()
{
    static const juce::StringArray a { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return a;
}

struct Scale { const char* name; std::vector<int> iv; };

inline const std::vector<Scale>& scales()
{
    static const std::vector<Scale> s {
        { "Moll (Aeolisch)", { 0, 2, 3, 5, 7, 8, 10 } },
        { "Dur (Ionisch)", { 0, 2, 4, 5, 7, 9, 11 } },
        { "Harmonisch Moll", { 0, 2, 3, 5, 7, 8, 11 } },
        { "Melodisch Moll", { 0, 2, 3, 5, 7, 9, 11 } },
        { "Dorisch", { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygisch", { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydisch", { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydisch", { 0, 2, 4, 5, 7, 9, 10 } },
        { "Lokrisch", { 0, 1, 3, 5, 6, 8, 10 } },
        { "Pentatonik Dur", { 0, 2, 4, 7, 9 } },
        { "Pentatonik Moll", { 0, 3, 5, 7, 10 } },
        { "Blues", { 0, 3, 5, 6, 7, 10 } },
        { "Ganzton", { 0, 2, 4, 6, 8, 10 } },
        { "Phrygisch Dominant", { 0, 1, 4, 5, 7, 8, 10 } },
        { "Ungarisch Moll", { 0, 2, 3, 6, 7, 8, 11 } },
        { "Hirajoshi", { 0, 2, 3, 7, 8 } },
        { "In (Japanisch)", { 0, 1, 5, 7, 8 } },
        { "Chromatisch", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } } };
    return s;
}

inline juce::Colour channelColour (int i)
{
    static const juce::uint32 c[NCH] { 0xffff6a2b, 0xffffb300, 0xffc2dd1f, 0xff27c98f,
                                       0xff25b9e8, 0xff5b82ff, 0xffad6bff, 0xffff5aa8 };
    return juce::Colour (c[juce::jlimit (0, NCH - 1, i)]);
}

// Shape of the waveforms that are defined in the time domain (phase p in [0,1)).
inline float waveFn (int w, double p)
{
    const double TAU = juce::MathConstants<double>::twoPi;
    switch (w)
    {
        case 0: return (float) std::sin (TAU * p);
        case 1: return (float) (p < 0.25 ? 4 * p : (p < 0.75 ? 2 - 4 * p : 4 * p - 4));
        case 2: return p < 0.5 ? 1.0f : -1.0f;
        case 3: return (float) (2 * p - 1);
        case 4: return (float) (1 - 2 * p);
        case 5: return p < 0.25 ? 1.0f : -1.0f;
        case 6: return p < 0.125 ? 1.0f : -1.0f;
        case 7: return p < 0.05 ? 1.0f : -1.0f;
        case 8: return p < 0.5 ? (float) std::sin (TAU * p) : 0.0f;
        case 9: return (float) std::abs (std::sin (TAU * p));
        case 10: return (float) juce::jlimit (-1.0, 1.0, 3.0 * (1.0 - 4.0 * std::abs (p - 0.5)));
        case 11: return (float) (std::round (std::sin (TAU * p) * 7.0) / 7.0);
        case 12: { const double x = 2 * p - 1; return (float) (1 - 2 * x * x); }
        case 13: return (float) std::sin (3.0 * std::sin (TAU * p));
        case 14: return (float) (std::fmod (p * 2.6, 1.0) * 2 - 1);
        default: return 0.0f;
    }
}

// Waveforms defined by harmonic amplitudes (sine partials, index 0 = fundamental).
inline const std::vector<float>* harmonicSpec (int w)
{
    static const std::vector<float> organ { 1, .8f, 0, .6f, 0, 0, 0, .45f, 0, 0, 0, .3f };
    static const std::vector<float> odd { 1, 0, .5f, 0, .33f, 0, .25f, 0, .2f };
    static const std::vector<float> bell { 1, 0, 0, .55f, 0, .4f, 0, 0, .3f, 0, 0, .22f, 0, 0, 0, .14f };
    static const std::vector<float> ah { .3f, .5f, .7f, 1, .9f, .6f, .35f, .2f, .15f, .3f, .4f, .3f, .15f, .08f };
    static const std::vector<float> brass { 1, .9f, .8f, .7f, .6f, .5f, .42f, .35f, .28f, .22f, .18f, .14f, .1f };
    switch (w)
    {
        case 15: return &organ;
        case 16: return &odd;
        case 17: return &bell;
        case 18: return &ah;
        case 19: return &brass;
        default: return nullptr;
    }
}

namespace colours
{
    const juce::Colour bg { 0xff121418 }, panel { 0xff1c1f25 }, panel2 { 0xff272b33 },
                       line { 0xff343945 }, ink { 0xffecebe7 }, mute { 0xff959aa4 },
                       knob { 0xff3b404b }, accent { 0xffff5a1f }, muteOn { 0xffe8b100 },
                       soloOn { 0xff1fb67a }, screen { 0xff0c0e11 };
}
} // namespace octo
