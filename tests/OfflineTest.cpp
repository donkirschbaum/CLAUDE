// Offline DSP test: renders every preset with a simulated 120 BPM host transport,
// checks for NaN/Inf, clipping and that the "Trance Gate" pattern really gates.
#include "PluginProcessor.h"
#include <cstdio>

namespace
{
    struct FakePlayHead : juce::AudioPlayHead
    {
        double ppq = 0.0, bpm = 120.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (bpm);
            info.setPpqPosition (ppq);
            info.setIsPlaying (true);
            return info;
        }
    };
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    const int bs = 512;
    int failures = 0;

    for (int preset = 0; preset < 6; ++preset)
    {
        PulseForgeProcessor p;
        FakePlayHead head;
        p.setPlayHead (&head);
        p.setPlayConfigDetails (2, 2, sr, bs);
        p.prepareToPlay (sr, bs);
        p.applyPreset (preset);

        const int total = (int) sr * 4;
        juce::AudioBuffer<float> rendered (2, total);
        juce::MidiBuffer midi;
        juce::Random rnd (1);
        double phase = 0.0;
        float peak = 0.0f;
        bool bad = false;

        for (int pos = 0; pos < total; pos += bs)
        {
            const int n = juce::jmin (bs, total - pos);
            juce::AudioBuffer<float> b (2, n);
            for (int i = 0; i < n; ++i)
            {
                phase += 110.0 / sr;
                if (phase > 1.0) phase -= 1.0;
                const float v = (float) (phase * 2.0 - 1.0) * 0.3f + (rnd.nextFloat() * 2.0f - 1.0f) * 0.05f;
                b.setSample (0, i, v);
                b.setSample (1, i, v);
            }
            p.processBlock (b, midi);
            head.ppq += n * head.bpm / 60.0 / sr;

            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    const float s = b.getSample (c, i);
                    if (! std::isfinite (s)) bad = true;
                    peak = std::max (peak, std::abs (s));
                    rendered.setSample (c, pos + i, s);
                }
        }

        juce::String rms;
        for (int st = 0; st < 8; ++st)
            rms << juce::String (rendered.getRMSLevel (0, st * 6000 + 500, 5000), 2) << " ";

        std::printf ("%-12s peak=%.3f finite=%s  1/16 RMS: %s\n",
                     p.getProgramName (preset).toRawUTF8(), peak, bad ? "NO" : "yes", rms.toRawUTF8());

        if (bad || peak > 4.0f) ++failures;

        if (preset == 1) // Trance Gate: steps 1,0,1,1,0,1,1,0
        {
            const int pattern[] { 1, 0, 1, 1, 0, 1, 1, 0 };
            for (int st = 0; st < 8; ++st)
            {
                const float r = rendered.getRMSLevel (0, st * 6000 + 1500, 3000);
                if (pattern[st] == 1 && r < 0.08f) { std::printf ("  gate step %d should be open\n", st + 1); ++failures; }
                if (pattern[st] == 0 && r > 0.08f) { std::printf ("  gate step %d should be closed\n", st + 1); ++failures; }
            }
        }
        p.setPlayHead (nullptr);
    }

    std::printf (failures == 0 ? "ALL TESTS PASSED\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
