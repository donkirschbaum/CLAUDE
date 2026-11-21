#include "Effects.h"
#include "DSP.h"

namespace ws
{

void EffectChain::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlockSize, 2 };
    chorus.prepare (spec);
    chorus.setCentreDelay (8.0f);
    chorus.setFeedback (0.1f);

    const size_t len = (size_t) (sr * 5.0) + 4;   // up to 5 s of delay
    delayL.assign (len, 0.0f);
    delayR.assign (len, 0.0f);

    reverb.setSampleRate (sr);
    gain.reset (sr, 0.05);
    reset();
}

void EffectChain::reset()
{
    chorus.reset();
    std::fill (delayL.begin(), delayL.end(), 0.0f);
    std::fill (delayR.begin(), delayR.end(), 0.0f);
    writePos = 0;
    delaySamplesSmoothed = 0.0f;
    toneL = toneR = 0.0f;
    reverb.reset();
}

void EffectChain::process (juce::AudioBuffer<float>& buffer, const FxSettings& s)
{
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    // --- Distortion
    if (s.distOn)
    {
        const float drive = 1.0f + s.distDrive * s.distDrive * 30.0f;
        const float comp = 1.0f / fastTanh (std::min (drive, 3.0f));
        for (int i = 0; i < n; ++i)
        {
            const float wl = fastTanh (L[i] * drive) * comp * 0.7f;
            const float wr = fastTanh (R[i] * drive) * comp * 0.7f;
            L[i] += (wl - L[i]) * s.distMix;
            R[i] += (wr - R[i]) * s.distMix;
        }
    }

    // --- Chorus
    if (s.chorusOn)
    {
        chorus.setRate (s.chorusRate);
        chorus.setDepth (s.chorusDepth);
        chorus.setMix (s.chorusMix);
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        chorus.process (ctx);
    }

    // --- Ping-pong delay
    if (s.delayOn && ! delayL.empty())
    {
        const int len = (int) delayL.size();
        const float target = juce::jlimit (16.0f, (float) (len - 4),
                                           (float) (s.delayBeats * 60.0 / std::max (20.0, s.bpm) * sampleRate));
        if (delaySamplesSmoothed <= 0.0f) delaySamplesSmoothed = target;
        const float toneCoef = 0.05f + 0.9f * s.delayTone;

        for (int i = 0; i < n; ++i)
        {
            delaySamplesSmoothed += (target - delaySamplesSmoothed) * 0.0005f;
            float rp = (float) writePos - delaySamplesSmoothed;
            while (rp < 0.0f) rp += (float) len;
            const int i0 = (int) rp;
            const int i1 = (i0 + 1) % len;
            const float fr = rp - (float) i0;
            const float dl = delayL[(size_t) i0] + fr * (delayL[(size_t) i1] - delayL[(size_t) i0]);
            const float dr = delayR[(size_t) i0] + fr * (delayR[(size_t) i1] - delayR[(size_t) i0]);

            toneL += toneCoef * (dl - toneL);
            toneR += toneCoef * (dr - toneR);

            const float in = 0.5f * (L[i] + R[i]);
            // ping-pong: mono input enters left, channels cross-feed
            delayL[(size_t) writePos] = fastTanh (in + toneR * s.delayFb);
            delayR[(size_t) writePos] = fastTanh (toneL * s.delayFb);
            writePos = (writePos + 1) % len;

            L[i] += dl * s.delayMix;
            R[i] += dr * s.delayMix;
        }
    }
    else
        delaySamplesSmoothed = 0.0f;

    // --- Reverb
    if (s.reverbOn)
    {
        juce::Reverb::Parameters p;
        p.roomSize = 0.3f + 0.69f * s.reverbSize;
        p.damping = s.reverbDamp;
        p.wetLevel = s.reverbMix * 0.8f;
        p.dryLevel = 1.0f - s.reverbMix * 0.5f;
        p.width = s.reverbWidth;
        p.freezeMode = 0.0f;
        reverb.setParameters (p);
        reverb.processStereo (L, R, n);
    }

    // --- Master gain + soft safety clipper
    gain.setTargetValue (s.masterGain);
    for (int i = 0; i < n; ++i)
    {
        const float g = gain.getNextValue();
        auto clip = [] (float x)
        {
            const float a = std::abs (x);
            if (a <= 0.9f) return x;
            const float over = a - 0.9f;
            const float y = 0.9f + 0.1f * fastTanh (over * 10.0f);
            return x < 0.0f ? -y : y;
        };
        L[i] = clip (L[i] * g);
        R[i] = clip (R[i] * g);
    }
}

} // namespace ws
