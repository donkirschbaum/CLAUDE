// LoopTape - Tape Echo + Layer Looper
// Copyright (C) 2026 donkirschbaum - AGPL-3.0 (see LICENSE)
//
// Tape-style stereo delay. JUCE-free so it can be unit tested on its own.
//  - delay-time changes glide like a tape motor (pitch bends, no clicks)
//  - wow (slow, drifting) + flutter (fast) modulation of the read head
//  - every pass through the record head is saturated and band-limited,
//    so repeats get darker and dirtier, and feedback > 100 % self-oscillates
//    without exploding (tanh ceiling)
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace lt
{
constexpr float kPi = 3.14159265358979f;

struct OnePoleLP
{
    float z = 0.0f, a = 0.0f;
    void setCutoff (float hz, float sr) { a = std::exp (-2.0f * kPi * std::min (hz, sr * 0.45f) / sr); }
    float process (float x) { z = x + (z - x) * a; return z; }
    void reset() { z = 0.0f; }
};

struct OnePoleHP
{
    OnePoleLP lp;
    void setCutoff (float hz, float sr) { lp.setCutoff (hz, sr); }
    float process (float x) { return x - lp.process (x); }
    void reset() { lp.reset(); }
};

// small fast PRNG (no allocation, real-time safe)
struct Rng
{
    uint32_t s = 0x12345678u;
    float next() // -1 .. 1
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
};

class TapeDelay
{
public:
    enum Mode { Stereo = 0, PingPong = 1 };

    struct Params
    {
        float timeMs     = 400.0f; // 20 .. 2500
        float feedback   = 0.4f;   // 0 .. 1.1
        float tone       = 0.6f;   // 0 (dark) .. 1 (bright)
        float lowCut     = 0.15f;  // 0 .. 1
        float wow        = 0.25f;  // 0 .. 1
        float flutter    = 0.2f;   // 0 .. 1
        float saturation = 0.35f;  // 0 .. 1
        float hiss       = 0.0f;   // 0 .. 1
        int   mode       = Stereo;
    };

    static constexpr float kMaxDelayMs = 2500.0f;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        const int needed = (int) (sr * (kMaxDelayMs + 200.0f) / 1000.0f) + 8;
        size = 1;
        while (size < needed) size <<= 1;
        mask = size - 1;
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        glideCoef = 1.0f - std::exp (-1.0f / (0.22f * sr)); // ~220 ms "motor" inertia
        reset();
    }

    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        writeIdx = 0;
        curDelay = -1.0f;
        for (int c = 0; c < 2; ++c) { lp1[c].reset(); lp2[c].reset(); hp[c].reset(); hissLp[c].reset(); }
        wowPhase = 0.0f; flutPhase = 0.0f; drift = 0.0f; driftTarget = 0.0f;
    }

    void setParams (const Params& np)
    {
        p = np;
        const float lpHz = 900.0f * std::pow (18000.0f / 900.0f, p.tone);
        const float hpHz = 20.0f * std::pow (900.0f / 20.0f, p.lowCut);
        for (int c = 0; c < 2; ++c)
        {
            lp1[c].setCutoff (lpHz, sr);
            lp2[c].setCutoff (lpHz * 1.4f, sr);
            hp[c].setCutoff (hpHz, sr);
            hissLp[c].setCutoff (7000.0f, sr);
        }
        drive = 1.0f + p.saturation * 1.8f;
        makeup = 1.0f + 0.45f * p.saturation; // applied to output only, never fed back
    }

    // in/out may alias
    void process (const float* inL, const float* inR, float* wetL, float* wetR, int n)
    {
        const float targetDelay = std::clamp (p.timeMs, 1.0f, kMaxDelayMs) * 0.001f * sr;
        if (curDelay < 0.0f) curDelay = targetDelay;

        const float wowDepth  = p.wow * p.wow * 0.0045f * sr;      // up to ~4.5 ms
        const float flutDepth = p.flutter * p.flutter * 0.0005f * sr; // up to ~0.5 ms
        const float wowInc  = 0.55f / sr;
        const float flutInc = 6.8f / sr;
        const float hissAmt = p.hiss * p.hiss * 0.008f;
        const float fb = p.feedback;

        for (int i = 0; i < n; ++i)
        {
            // tape motor glide
            curDelay += (targetDelay - curDelay) * glideCoef;

            // wow / flutter
            wowPhase += wowInc;  if (wowPhase >= 1.0f) wowPhase -= 1.0f;
            flutPhase += flutInc; if (flutPhase >= 1.0f) flutPhase -= 1.0f;
            if (--driftCountdown <= 0)
            {
                driftTarget = rng.next();
                driftCountdown = (int) (sr * 0.35f);
            }
            drift += (driftTarget - drift) * (3.0f / sr);

            const float wowL = std::sin (2.0f * kPi * wowPhase) * 0.7f + drift * 0.3f;
            const float wowR = std::sin (2.0f * kPi * wowPhase + 0.9f) * 0.7f + drift * 0.3f;
            const float flL  = std::sin (2.0f * kPi * flutPhase) + rng.next() * 0.15f;
            const float flR  = std::sin (2.0f * kPi * flutPhase + 1.7f) + rng.next() * 0.15f;

            const float dL = curDelay + wowDepth * (1.0f + wowL) + flutDepth * (1.0f + flL);
            const float dR = curDelay + wowDepth * (1.0f + wowR) + flutDepth * (1.0f + flR);

            float rL = read (0, dL);
            float rR = read (1, dR);

            if (hissAmt > 0.0f)
            {
                rL += hissLp[0].process (rng.next()) * hissAmt;
                rR += hissLp[1].process (rng.next()) * hissAmt;
            }

            float wL, wR;
            if (p.mode == PingPong)
            {
                wL = 0.5f * (inL[i] + inR[i]) + fb * rR;
                wR = fb * rL;
            }
            else
            {
                wL = inL[i] + fb * rL;
                wR = inR[i] + fb * rR;
            }

            buf[0][(size_t) writeIdx] = recordHead (0, wL);
            buf[1][(size_t) writeIdx] = recordHead (1, wR);
            writeIdx = (writeIdx + 1) & mask;

            wetL[i] = rL * makeup;
            wetR[i] = rR * makeup;
        }
    }

private:
    float recordHead (int c, float x)
    {
        x = hp[c].process (x);
        x = lp2[c].process (lp1[c].process (x));
        // soft tape saturation, ceiling 1/drive
        // (small-signal gain is exactly 1, so loop gain == feedback knob)
        return std::tanh (x * drive) / drive;
    }

    float read (int c, float delaySamples) const
    {
        const float pos = (float) writeIdx - delaySamples;
        const float fl = std::floor (pos);
        const float t = pos - fl;
        const int i1 = (int) fl;
        const auto& b = buf[c];
        const float y0 = b[(size_t) ((i1 - 1) & mask)];
        const float y1 = b[(size_t) (i1 & mask)];
        const float y2 = b[(size_t) ((i1 + 1) & mask)];
        const float y3 = b[(size_t) ((i1 + 2) & mask)];
        // 4-point Hermite
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + c0;
    }

    float sr = 44100.0f;
    int size = 0, mask = 0, writeIdx = 0;
    std::vector<float> buf[2];
    Params p;
    float curDelay = -1.0f, glideCoef = 0.0f, drive = 1.0f, makeup = 1.0f;
    OnePoleLP lp1[2], lp2[2], hissLp[2];
    OnePoleHP hp[2];
    float wowPhase = 0.0f, flutPhase = 0.0f, drift = 0.0f, driftTarget = 0.0f;
    int driftCountdown = 0;
    Rng rng;
};
} // namespace lt
