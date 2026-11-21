#pragma once

#include "SynthSettings.h"
#include "DSP.h"
#include "Wavetables.h"

namespace ws
{

class Voice
{
public:
    static constexpr int maxUnison = 8;

    void prepare (double sampleRate, uint32_t seed);

    void start (int note, float velocity, float glideFromNote, const SynthSettings& s);
    void legatoTo (int note);                 // change pitch without retriggering
    void release();
    void kill();

    bool isActive() const noexcept   { return active; }
    bool isReleased() const noexcept { return released; }
    int  getNote() const noexcept    { return note; }
    uint64_t getAge() const noexcept { return age; }
    float getLevel() const noexcept  { return ampEnv.getLevel(); }

    bool sustained = false;                   // held by sustain pedal

    // Renders and *adds* n samples to the output buffers (n <= 32)
    void render (float* outL, float* outR, int n, const SynthSettings& s, const GlobalMod& g);

private:
    void resetOscillators (const SynthSettings& s);

    const WavetableBank& bank = WavetableBank::get();
    float sampleRate = 44100.0f;
    Random rng;

    bool active = false, released = false;
    int note = 60;
    float velocity = 1.0f, noteRandom = 0.0f;
    float currentPitch = 60.0f, targetPitch = 60.0f;
    uint64_t age = 0;
    static inline uint64_t ageCounter = 0;

    Envelope ampEnv, modEnv;

    float phase[2][maxUnison] {};
    float subPhase = 0.0f;
    float noiseLp = 0.0f;

    float lfoPhase[2] { 0, 0 };
    float lfoHeld[2] { 0, 0 }, lfoPrev[2] { 0, 0 };

    SvfState svf[2][2];   // [channel][stage]
};

} // namespace ws
