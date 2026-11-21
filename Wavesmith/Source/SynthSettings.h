#pragma once

#include "Parameters.h"

namespace ws
{

struct OscSettings
{
    bool on = true;
    int table = 0;
    float pos = 0, fine = 0, detune = 0.2f, spread = 0.7f, level = 0.75f, pan = 0, phase = 0;
    int oct = 0, semi = 0, uni = 1;
    bool randPhase = true;
};

struct LfoSettings
{
    int shape = 0;
    float rateHz = 1.0f;
    bool retrig = true;
};

struct MatrixSlot
{
    int src = 0, dst = 0;
    float amt = 0.0f;
};

// Snapshot of all sound parameters, refreshed once per audio block
struct SynthSettings
{
    OscSettings osc[2];
    float fm = 0;
    float subLevel = 0; int subOct = 0, subShape = 0;
    float noiseLevel = 0, noiseColor = 0.5f;

    bool fltOn = true; int fltType = 1;
    float cutoff = 12000, reso = 0.1f, drive = 0, fltEnv = 0, fltKey = 0, fltVel = 0;

    float ampA = 0.002f, ampD = 0.3f, ampS = 0.8f, ampR = 0.25f;
    float envA = 0.002f, envD = 0.4f, envS = 0, envR = 0.3f;
    float ampVel = 0.4f;

    LfoSettings lfo[2];
    MatrixSlot matrix[numMatrixSlots];

    int polyMode = 0, voices = 12, bendRange = 2;
    float glide = 0;
};

// Values shared by all voices, updated once per processing chunk
struct GlobalMod
{
    float seqValue = 0;
    float modWheel = 0;
    float aftertouch = 0;
    float pitchBend = 0;              // -1..1
    float lfoPhase[2] { 0, 0 };       // free running phase for non-retriggered LFOs
    float lfoHeld[2] { 0, 0 };
    float lfoPrevHeld[2] { 0, 0 };
};

} // namespace ws
