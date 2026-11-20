// Rendert ein paar Takte ohne Host und prüft, ob sinnvoll Audio herauskommt.
#include <juce_audio_formats/juce_audio_formats.h>
#include "../src/Engine.h"
#include "../src/Generator.h"
#include <cstdio>

using namespace shaker;

int main (int argc, char** argv)
{
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    SampleLibrary lib;
    std::printf ("Samples geladen: %d\n", lib.size());
    if (lib.size() < 4) return 1;

    Engine eng (lib);
    const double sr = 48000.0;
    const int block = 512;
    eng.prepare (sr, block);

    juce::Random rng (1234);
    auto picks = Generator::pickSamples (lib, rng);
    TrackParams tp[kTracks];
    GlobalParams g;
    for (int t = 0; t < kTracks; ++t)
    {
        auto* s = lib.get (picks[(size_t) t]);
        eng.shared[t].sample.store (picks[(size_t) t]);
        eng.shared[t].start.store (s->onset);
        eng.shared[t].end.store ((float) s->lengthSeconds());
        TrackConfig cfg;
        float pat[kMaxSteps];
        Generator::genRole (t, true, cfg, pat, 0, 0, rng);
        for (int i = 0; i < kMaxSteps; ++i) eng.shared[t].pat[(size_t) i].store (pat[i]);
        tp[t].steps = cfg.steps; tp[t].mode = cfg.mode; tp[t].rate = cfg.rate;
        tp[t].vol = Generator::roleMix[t].vol; tp[t].pan = Generator::roleMix[t].pan;
        tp[t].decay = 0.5f * (Generator::roleMix[t].decMin + Generator::roleMix[t].decMax);
        std::printf ("Spur %d: %-24s steps %2d mode %s rate %s cycle %.3f beats\n", t + 1, s->name.toRawUTF8(), cfg.steps,
                     cfg.mode == Polymeter ? "PM" : "PR", kRates[cfg.rate].label, cycleBeats (cfg.steps, cfg.mode, cfg.rate));
    }

    // Test 1: 8 Takte bei 96 BPM, Test 2: Chaos/Raum/Ratchets an
    for (int pass = 0; pass < 2; ++pass)
    {
        if (pass == 1)
        {
            g.chaos = 1.0f; g.room = 0.4f; g.roomSize = 0.5f; g.evolve = 0.3f;
            for (auto& p : tp) { p.rRatchet = 0.4f; p.rRev = 0.3f; p.rPitch = 0.3f; p.comp = 0.5f; p.drive = 0.4f; p.lowcut = 200; p.highcut = 9000; }
        }
        const double bpm = 96.0;
        const int total = (int) (sr * 60.0 / bpm * 4 * 8);
        juce::AudioBuffer<float> all (2, total);
        juce::AudioBuffer<float> buf (2, block);
        double ppq = 0;
        unsigned int hits0[kTracks];
        for (int t = 0; t < kTracks; ++t) hits0[t] = eng.shared[t].hits.load();
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            juce::AudioBuffer<float> b (buf.getArrayOfWritePointers(), 2, n);
            Transport tr { true, ppq, bpm };
            eng.process (b, tr, g, tp);
            for (int c = 0; c < 2; ++c) all.copyFrom (c, pos, b, c, 0, n);
            ppq += n / (sr * 60.0 / bpm);
        }
        float pk = all.getMagnitude (0, total);
        float rms = (all.getRMSLevel (0, 0, total) + all.getRMSLevel (1, 0, total)) * 0.5f;
        bool bad = false;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < total; ++i)
                if (! std::isfinite (all.getSample (c, i))) bad = true;
        std::printf ("Pass %d: Spitze %.3f, RMS %.4f, NaN %s, Hits:", pass, pk, rms, bad ? "JA" : "nein");
        for (int t = 0; t < kTracks; ++t) std::printf (" %u", eng.shared[t].hits.load() - hits0[t]);
        std::printf ("\n");

        auto f = outDir.getChildFile (juce::String ("render_") + juce::String (pass) + ".wav");
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 16, {}, 0));
        if (w) w->writeFromAudioSampleBuffer (all, 0, total);
        if (bad || pk <= 0.001f) return 2;
    }
    std::printf ("OK\n");
    return 0;
}
