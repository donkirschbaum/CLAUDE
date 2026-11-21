#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace ws
{

constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717958f;

inline float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline float clampf (float x, float lo, float hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }
inline float semitoneRatio (float st) noexcept { return std::exp2 (st / 12.0f); }
inline float midiToHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }

inline float fastTanh (float x) noexcept
{
    x = clampf (x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Simple xorshift PRNG, real-time safe
struct Random
{
    uint32_t state = 0x12345678u;
    void seed (uint32_t s) noexcept { state = s ? s : 0x9e3779b9u; }
    uint32_t next() noexcept { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; }
    float uni() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // 0..1
    float bi() noexcept  { return uni() * 2.0f - 1.0f; }                             // -1..1
};

//==============================================================================
// ADSR with linear attack and exponential decay / release
class Envelope
{
public:
    enum class Stage { idle, attack, decay, sustain, release };

    void setSampleRate (double sr) noexcept { sampleRate = (float) sr; }
    void setParams (float a, float d, float s, float r) noexcept
    {
        attackInc = 1.0f / std::max (1.0f, a * sampleRate);
        decayCoef = std::exp (-4.6f / std::max (1.0f, d * sampleRate));
        sustain = s;
        releaseCoef = std::exp (-6.9f / std::max (1.0f, r * sampleRate));
    }
    void noteOn() noexcept  { stage = Stage::attack; }        // retriggers from current level
    void noteOff() noexcept { if (stage != Stage::idle) stage = Stage::release; }
    void reset() noexcept   { stage = Stage::idle; level = 0.0f; }
    bool isActive() const noexcept { return stage != Stage::idle; }
    bool isReleasing() const noexcept { return stage == Stage::release; }
    float getLevel() const noexcept { return level; }

    float process() noexcept
    {
        switch (stage)
        {
            case Stage::attack:
                level += attackInc;
                if (level >= 1.0f) { level = 1.0f; stage = Stage::decay; }
                break;
            case Stage::decay:
                level = sustain + (level - sustain) * decayCoef;
                if (std::abs (level - sustain) < 1.0e-4f) { level = sustain; stage = Stage::sustain; }
                break;
            case Stage::sustain:
                level = sustain;
                break;
            case Stage::release:
                level *= releaseCoef;
                if (level < 1.0e-5f) { level = 0.0f; stage = Stage::idle; }
                break;
            case Stage::idle:
            default:
                break;
        }
        return level;
    }

private:
    Stage stage = Stage::idle;
    float level = 0.0f, sampleRate = 44100.0f;
    float attackInc = 1.0f, decayCoef = 0.0f, sustain = 1.0f, releaseCoef = 0.0f;
};

//==============================================================================
// Topology-preserving-transform state variable filter (Simper / Zavalishin)
struct SvfCoeffs
{
    float a1 = 0, a2 = 0, a3 = 0, k = 2;
    void set (float cutoffHz, float k_, float sampleRate) noexcept
    {
        const float fc = clampf (cutoffHz, 10.0f, sampleRate * 0.45f);
        const float g = std::tan (kPi * fc / sampleRate);
        k = k_;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};

struct SvfState
{
    float ic1 = 0, ic2 = 0;
    void reset() noexcept { ic1 = ic2 = 0; }

    // returns low, band, high via references
    inline void process (float v0, const SvfCoeffs& c, float& low, float& band, float& high) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = c.a1 * ic1 + c.a2 * v3;
        const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        low = v2; band = v1; high = v0 - c.k * v1 - v2;
    }
};

//==============================================================================
// LFO shapes, phase 0..1, bipolar output
inline float lfoShape (int shape, float phase, float held, float prevHeld) noexcept
{
    switch (shape)
    {
        case 0: return std::sin (kTwoPi * phase);
        case 1: return phase < 0.5f ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
        case 2: return 2.0f * phase - 1.0f;
        case 3: return 1.0f - 2.0f * phase;
        case 4: return phase < 0.5f ? 1.0f : -1.0f;
        case 5: return held;
        default:
        {
            const float w = 0.5f - 0.5f * std::cos (kPi * phase);
            return prevHeld + (held - prevHeld) * w;
        }
    }
}

} // namespace ws
