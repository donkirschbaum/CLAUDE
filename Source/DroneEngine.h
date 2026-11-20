#pragma once
// Drone Acht – DSP-Kern.
// 8 Dreieck-Oszillatoren in 4 Paaren / 2 Quads, Kreuz-FM, Hyper-LFO,
// kreuzgekoppeltes Ping-Pong-Delay mit LFO-Modulation, Verzerrung.
// Ohne JUCE-Abhängigkeit, damit der Kern auch woanders nutzbar ist.

#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include <algorithm>

namespace drone
{

constexpr int   numVoices = 8;
constexpr float twoPi     = 6.28318530718f;
constexpr float halfPi    = 1.57079632679f;

enum ModSource { srcPair = 0, srcNeighbour = 1, srcLfo = 2 };
enum LfoMode   { lfoMix = 0, lfoRing = 1 };
enum LfoWave   { waveTriangle = 0, waveSquare = 1 };

// Grundstimmung: Halbtöne über A1 (55 Hz)
constexpr std::array<float, numVoices> baseSemis { 0, 0, 7, 7, 12, 12, 19, 19 };
constexpr std::array<float, numVoices> voicePan  { -.7f, -.45f, -.25f, -.1f, .1f, .25f, .45f, .7f };

struct Params
{
    float vol = .7f, drive = .18f, vib = .08f, vibRate = 4.6f;
    float lfoA = .23f, lfoB = 3.1f;
    int   lfoMode = lfoMix, lfoWave = waveTriangle;
    float t1 = .37f, t2 = .61f, fb = .45f, dmod = .25f, dmix = .35f;
    float hold[2]  { .45f, 0.f };
    float pitch[2] { 0.f, 0.f };
    int   src[2]   { srcPair, srcPair };
    float mod[4]   { .14f, .22f, .1f, .28f };
    float sharp[4] { .15f, .3f, .05f, .45f };
    float tune[numVoices] { 0.f, .08f, 0.f, .06f, 0.f, .05f, 0.f, .09f };
    bool  fast[numVoices] { true, true, true, true, false, false, false, false };
};

inline float voiceFrequency (const Params& p, int i)
{
    return 55.f * std::pow (2.f, (baseSemis[(size_t) i] + p.tune[i] + p.pitch[i >> 2]) / 12.f);
}

// Einpoliger Glätter für Parameter
struct Smooth
{
    float cur = 0.f, target = 0.f, coef = 0.f;
    void setTime (float seconds, double sr) { coef = std::exp (-1.f / (float) (seconds * sr)); }
    void reset (float v)                     { cur = target = v; }
    inline float next()                      { cur = target + (cur - target) * coef; return cur; }
};

class DelayLine
{
public:
    void prepare (int maxSamples) { buf.assign ((size_t) maxSamples + 4, 0.f); w = 0; }
    void clear()                  { std::fill (buf.begin(), buf.end(), 0.f); }

    inline float read (float delaySamples) const
    {
        const int size = (int) buf.size();
        delaySamples = std::clamp (delaySamples, 1.f, (float) size - 3.f);
        float pos = (float) w - delaySamples;
        if (pos < 0.f) pos += (float) size;
        const int   i0 = (int) pos;
        const int   i1 = (i0 + 1) % size;
        const float fr = pos - (float) i0;
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * fr;
    }
    inline void write (float x)
    {
        buf[(size_t) w] = x;
        if (++w >= (int) buf.size()) w = 0;
    }

private:
    std::vector<float> buf;
    int w = 0;
};

class Engine
{
public:
    Engine()
    {
        for (auto& l : level) l.store (0.f);
    }

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        d1.prepare ((int) (3.0 * sampleRate));
        d2.prepare ((int) (3.0 * sampleRate));

        for (auto* s : { &outGain, &drvGain, &vibCents, &vibRate, &lfoARate, &lfoBRate,
                         &aMix, &bMix, &ringMix, &fbGain, &dModSamples, &wetGain, &dryGain })
            s->setTime (.03f, sampleRate);
        t1Samples.setTime (.15f, sampleRate);
        t2Samples.setTime (.15f, sampleRate);

        for (int i = 0; i < numVoices; ++i)
        {
            freq[i].setTime (.02f, sampleRate);
            modDepth[i].setTime (.03f, sampleRate);
            preGain[i].setTime (.03f, sampleRate);

            const float x = (voicePan[(size_t) i] + 1.f) * .5f;   // gleichleistendes Panning
            panL[i] = std::cos (x * halfPi);
            panR[i] = std::sin (x * halfPi);
        }

        const float lpHz = 3200.f;
        lpCoef = std::exp (-twoPi * lpHz / sr);
        firstBlock = true;
    }

    void reset()
    {
        d1.clear(); d2.clear();
        for (int i = 0; i < numVoices; ++i) { phase[i] = 0.f; env[i] = 0.f; prev[i] = 0.f; level[(size_t) i].store (0.f); }
        lpState = 0.f;
    }

    // touching[i]: Stimme i wird gerade gespielt (Pad, Taste oder MIDI)
    void process (const Params& p, const bool* touching, float* outL, float* outR, int numSamples)
    {
        setTargets (p, touching);
        if (firstBlock) { snapAll(); firstBlock = false; }

        const float dist3 = std::tanh (3.f);

        for (int n = 0; n < numSamples; ++n)
        {
            // Vibrato
            vibPhase += vibRate.next() / sr;
            vibPhase -= std::floor (vibPhase);
            const float detune = std::exp2 (vibCents.next() * std::sin (twoPi * vibPhase) / 1200.f);

            // Hyper-LFO
            lfoAPhase += lfoARate.next() / sr; lfoAPhase -= std::floor (lfoAPhase);
            lfoBPhase += lfoBRate.next() / sr; lfoBPhase -= std::floor (lfoBPhase);
            const float a   = shape (lfoAPhase, p.lfoWave);
            const float b   = shape (lfoBPhase, p.lfoWave);
            const float lfo = a * aMix.next() + b * bMix.next() + a * b * ringMix.next();

            // Stimmen
            float busL = 0.f, busR = 0.f;
            float shaped[numVoices];

            for (int i = 0; i < numVoices; ++i)
            {
                const int   q = i >> 2;
                const float modSig = p.src[q] == srcLfo       ? lfo
                                   : p.src[q] == srcNeighbour ? prev[i ^ 2]
                                                              : prev[i ^ 1];

                const float f = freq[i].next() * detune + modDepth[i].next() * modSig;
                phase[i] += f / sr;
                phase[i] -= std::floor (phase[i]);

                const float tri = 1.f - 4.f * std::abs (phase[i] - .5f);
                const float x   = std::clamp (tri * preGain[i].next(), -1.f, 1.f);
                const float y   = std::sin (x * halfPi);
                shaped[i] = y;

                env[i] = envTarget[i] + (env[i] - envTarget[i]) * envCoef[i];
                const float v = y * env[i] * .13f;
                busL += v * panL[i];
                busR += v * panR[i];
            }
            std::copy (shaped, shaped + numVoices, prev);

            // Delay: d1 -> Tiefpass -> d2, d2 -> d1
            const float dm  = dModSamples.next() * lfo;
            const float o1  = d1.read (t1Samples.next() + dm);
            const float o2  = d2.read (t2Samples.next() + dm);
            const float in  = (busL + busR) * .6f;
            const float fb  = fbGain.next();
            lpState = o1 + (lpState - o1) * lpCoef;
            d1.write (in + fb * o2);
            d2.write (in + fb * lpState);

            const float wet = wetGain.next(), dry = dryGain.next();
            float L = busL * dry + wet * (o1 * .988f + o2 * .156f);
            float R = busR * dry + wet * (o1 * .156f + o2 * .988f);

            // Verzerrung + Ausgang
            const float drv = drvGain.next(), out = outGain.next();
            L = std::tanh (3.f * std::clamp (L * drv, -1.f, 1.f)) / dist3 * out;
            R = std::tanh (3.f * std::clamp (R * drv, -1.f, 1.f)) / dist3 * out;

            outL[n] = L;
            outR[n] = R;
        }

        for (int i = 0; i < numVoices; ++i)
            level[(size_t) i].store (env[i], std::memory_order_relaxed);
    }

    std::array<std::atomic<float>, numVoices> level;   // Hüllkurvenstand für die Oberfläche

private:
    static inline float shape (float ph, int wave)
    {
        return wave == waveSquare ? (ph < .5f ? 1.f : -1.f) : 1.f - 4.f * std::abs (ph - .5f);
    }

    void setTargets (const Params& p, const bool* touching)
    {
        outGain.target   = p.vol * p.vol * .9f;
        drvGain.target   = 1.f + p.drive * p.drive * 30.f;
        vibCents.target  = p.vib * p.vib * 70.f;
        vibRate.target   = p.vibRate;
        lfoARate.target  = p.lfoA;
        lfoBRate.target  = p.lfoB;
        const bool ring  = p.lfoMode == lfoRing;
        aMix.target      = ring ? 0.f : .5f;
        bMix.target      = ring ? 0.f : .5f;
        ringMix.target   = ring ? 1.f : 0.f;
        t1Samples.target = p.t1 * sr;
        t2Samples.target = p.t2 * sr;
        fbGain.target    = p.fb;
        dModSamples.target = p.dmod * p.dmod * .016f * sr;
        wetGain.target   = p.dmix;
        dryGain.target   = 1.f - p.dmix * .45f;

        for (int i = 0; i < numVoices; ++i)
        {
            const int   q   = i >> 2, pr = i >> 1;
            const float f   = voiceFrequency (p, i);
            const float amt = p.mod[pr] * p.mod[pr];
            freq[i].target     = f;
            modDepth[i].target = p.src[q] == srcLfo ? amt * f * .6f : amt * f * 2.5f;
            preGain[i].target  = 1.f + std::pow (p.sharp[pr], 1.5f) * 12.f;

            const bool on = touching[i];
            envTarget[i]  = on ? 1.f : std::pow (p.hold[q], 1.3f) * .85f;
            const float tau = on ? (p.fast[i] ? .006f : .55f) : (p.fast[i] ? .09f : 1.2f);
            envCoef[i] = std::exp (-1.f / (tau * sr));
        }
    }

    void snapAll()
    {
        for (auto* s : { &outGain, &drvGain, &vibCents, &vibRate, &lfoARate, &lfoBRate,
                         &aMix, &bMix, &ringMix, &fbGain, &dModSamples, &wetGain, &dryGain,
                         &t1Samples, &t2Samples })
            s->reset (s->target);
        for (int i = 0; i < numVoices; ++i)
        {
            freq[i].reset (freq[i].target);
            modDepth[i].reset (modDepth[i].target);
            preGain[i].reset (preGain[i].target);
        }
    }

    float sr = 44100.f;
    bool  firstBlock = true;

    Smooth outGain, drvGain, vibCents, vibRate, lfoARate, lfoBRate, aMix, bMix, ringMix;
    Smooth t1Samples, t2Samples, fbGain, dModSamples, wetGain, dryGain;
    Smooth freq[numVoices], modDepth[numVoices], preGain[numVoices];

    float phase[numVoices] {}, env[numVoices] {}, prev[numVoices] {};
    float envTarget[numVoices] {}, envCoef[numVoices] {};
    float panL[numVoices] {}, panR[numVoices] {};

    float vibPhase = 0.f, lfoAPhase = 0.f, lfoBPhase = .25f;
    DelayLine d1, d2;
    float lpState = 0.f, lpCoef = 0.f;
};

} // namespace drone
