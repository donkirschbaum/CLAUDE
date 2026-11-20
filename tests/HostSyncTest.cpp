// Drives the real LoopTapeProcessor with a simulated host transport.
#include "../Source/PluginProcessor.h"
#include <cstdio>

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { std::printf ("FAIL: " __VA_ARGS__); std::printf ("\n"); ++failures; } \
                              else { std::printf ("ok:   " __VA_ARGS__); std::printf ("\n"); } } while (0)

struct FakeHost : juce::AudioPlayHead
{
    double bpm = 120.0, ppq = 0.0; bool playing = true; int num = 4, den = 4;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm); p.setPpqPosition (ppq); p.setIsPlaying (playing);
        p.setTimeSignature (TimeSignature { num, den });
        const double barQ = num * 4.0 / den;
        p.setPpqPositionOfLastBarStart (std::floor (ppq / barQ) * barQ);
        return p;
    }
};

constexpr double SR = 48000.0;
constexpr int BS = 512;

struct Rig
{
    LoopTapeProcessor proc;
    FakeHost host;
    juce::AudioBuffer<float> buf { 2, BS };
    juce::MidiBuffer midi;
    std::function<float (double)> input = [] (double) { return 0.0f; }; // input as a function of host ppq
    std::vector<float> outL;                                             // last block output
    std::vector<double> outPpq;

    Rig()
    {
        proc.setPlayHead (&host);
        proc.prepareToPlay (SR, BS);
        set ("mix", 0.0f);      // no echo in this test
        set ("killDry", 1.0f);  // output = loop only
        set ("hostSync", 1.0f);
    }
    void set (const char* id, float plain)
    {
        auto* p = proc.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }
    void block()
    {
        const double qps = host.bpm / 60.0 / SR;
        outPpq.resize (BS);
        for (int i = 0; i < BS; ++i)
        {
            const double q = host.ppq + (host.playing ? i * qps : 0.0);
            buf.setSample (0, i, input (q)); buf.setSample (1, i, input (q));
            outPpq[(size_t) i] = q;
        }
        proc.processBlock (buf, midi);
        outL.assign (buf.getReadPointer (0), buf.getReadPointer (0) + BS);
        if (host.playing) host.ppq += BS * qps;
        proc.timerCallbackForTest();
    }
    void run (double seconds) { for (int b = 0; b < (int) (seconds * SR / BS); ++b) block(); }
    int state() const { return proc.info.state.load(); }
};

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI init;
    using S = lt::Looper::State;

    // marker input: value encodes the host position (quarter notes / 1000)
    auto marker = [] (double q) { return (float) (q / 1000.0); };

    // ------------------------------------------------------- arm -> start on next bar, fixed 1 bar
    {
        Rig r;
        r.set ("loopBars", 1.0f); // index 1 = "1 Takt"
        r.input = marker;
        r.host.ppq = 0.3;
        r.proc.sendCommand (LoopTapeProcessor::CmdRecord);
        r.block();
        CHECK (r.state() == (int) S::Empty && r.proc.info.hostArmed.load(), "REC arms instead of recording mid-bar");
        r.run (1.6); // ppq 0.3 -> ~3.5
        CHECK (r.state() == (int) S::Empty, "still waiting before bar line");
        r.run (0.5); // crosses ppq 4.0
        CHECK (r.state() == (int) S::Recording, "recording started at the bar line");
        r.run (2.0);
        CHECK (r.state() == (int) S::Playing, "fixed 1-bar take closed itself");
        const double lenSec = r.proc.info.loopSeconds.load();
        CHECK (std::abs (lenSec - 2.0) < 1e-6, "loop = exactly 1 bar at 120 BPM (%.6f s)", lenSec);

        juce::AudioBuffer<float> raw;
        r.proc.renderLoop (raw, -1);
        const float v = raw.getSample (0, 1000) * 1000.0f; // after fade-in
        const double expect = 4.0 + 1000.0 * 120.0 / 60.0 / SR;
        CHECK (std::abs (v - expect) < 2e-3, "first loop sample recorded at ppq 4.0 (got %.4f, want %.4f)", v, expect);

        // locked playback: output at host position P == recording at 4 + (P-4) mod 4
        r.input = [] (double) { return 0.0f; };
        auto checkLock = [&] (const char* what) {
            r.block();
            double maxErr = 0;
            for (int i = 50; i < BS - 50; ++i)
            {
                const double q = r.outPpq[(size_t) i];
                const double want = 4.0 + std::fmod (q - 4.0, 4.0);
                if (want < 4.05 || want > 7.95) continue; // skip seam fades
                maxErr = std::max (maxErr, std::abs (r.outL[(size_t) i] * 1000.0 - want));
            }
            CHECK (maxErr < 0.01, "%s (max err %.4f quarters)", what, maxErr);
        };
        r.run (1.0);
        checkLock ("loop phase locked to song position");
        r.host.ppq = 101.37;  r.run (0.05);
        checkLock ("follows a jump in the song position");
        r.host.bpm = 97.0; r.run (0.3);
        checkLock ("stays on the grid after a tempo change");
        r.host.bpm = 120.0;

        r.host.playing = false; r.run (0.1);
        float mx = 0; for (float x : r.outL) mx = std::max (mx, std::abs (x));
        CHECK (mx < 1e-6, "silent while Ableton is stopped");
        r.host.playing = true; r.host.ppq = 8.0; r.run (0.2);
        checkLock ("resumes in sync after play");

        // half speed: loop spans 2 bars of host time
        r.set ("halfSpeed", 1.0f); r.run (0.2); r.block();
        double maxErr = 0;
        for (int i = 50; i < BS - 50; ++i)
        {
            const double q = r.outPpq[(size_t) i];
            const double want = 4.0 + std::fmod (q - 4.0, 8.0) * 0.5;
            if (want < 4.05 || want > 7.95) continue;
            maxErr = std::max (maxErr, std::abs (r.outL[(size_t) i] * 1000.0 - want));
        }
        CHECK (maxErr < 0.01, "1/2 speed: locked, loop spans 2 bars (err %.4f)", maxErr);

        // export = full looper output (1/2 speed -> twice as long)
        juce::AudioBuffer<float> out;
        CHECK (r.proc.renderOutput (out, -1) && out.getNumSamples() == raw.getNumSamples() * 2,
               "export at 1/2 speed is twice as long (%d vs %d)", out.getNumSamples(), raw.getNumSamples());
        CHECK (std::abs (out.getSample (0, 4000) - raw.getSample (0, 2000)) < 1e-4, "export at 1/2 speed plays the loop at half rate");
        r.set ("loopLevel", 0.5f);
        r.proc.renderOutput (out, -1);
        CHECK (std::abs (out.getSample (0, 4000) - 0.5f * raw.getSample (0, 2000)) < 1e-4, "export includes loop level");
        r.set ("loopLevel", 1.0f); r.set ("halfSpeed", 0.0f); r.set ("reverse", 1.0f);
        r.proc.renderOutput (out, -1);
        CHECK (out.getNumSamples() == raw.getNumSamples()
                   && std::abs (out.getSample (0, 1000) - raw.getSample (0, raw.getNumSamples() - 1001)) < 1e-4,
               "export in reverse plays backwards");
        r.set ("reverse", 0.0f);

        // overdub while locked: new layer lines up with the grid
        r.input = [] (double q) { return std::fmod (q, 4.0) < 2.0 ? 0.25f : 0.0f; }; // first half of each bar
        r.host.ppq = 16.0; r.run (0.1);
        r.proc.sendCommand (LoopTapeProcessor::CmdRecord); r.block();
        CHECK (r.state() == (int) S::Overdubbing, "overdub while host-locked");
        r.run (2.2);
        r.proc.sendCommand (LoopTapeProcessor::CmdRecord); r.run (0.1);
        juce::AudioBuffer<float> l2;
        r.proc.renderLoop (l2, 1);
        const float a = l2.getSample (0, (int) (0.5 * SR)), b = l2.getSample (0, (int) (1.5 * SR));
        CHECK (std::abs (a - 0.25f) < 1e-3 && std::abs (b) < 1e-3, "overdub layer aligned to bar (%.3f / %.3f)", a, b);

        // export with Feed Delay contains echoes
        r.set ("routing", 1.0f); r.set ("mix", 0.5f);
        juce::AudioBuffer<float> withEcho, noEcho;
        r.proc.renderOutput (withEcho, -1);
        r.set ("routing", 0.0f);
        r.proc.renderOutput (noEcho, -1);
        double diff = 0; for (int i = 0; i < noEcho.getNumSamples(); ++i) diff += std::abs (withEcho.getSample (0, i) - noEcho.getSample (0, i));
        CHECK (diff > 10.0, "Feed Delay routing: export contains the echoes");
    }

    // ------------------------------------------------------- free + host: close on nearest bar
    {
        Rig r;
        r.set ("loopBars", 0.0f); // Frei
        r.input = [] (double) { return 0.1f; };
        r.host.ppq = 0.0;
        r.proc.sendCommand (LoopTapeProcessor::CmdRecord); r.block();
        CHECK (r.state() == (int) S::Recording, "transport at bar line -> starts immediately");
        r.run (4.3); // ~2.15 bars at 120
        r.proc.sendCommand (LoopTapeProcessor::CmdRecord); r.run (0.3);
        CHECK (r.state() == (int) S::Playing && std::abs (r.proc.info.loopSeconds.load() - 4.0) < 1e-6,
               "free length rounded to 2 bars (%.4f s)", r.proc.info.loopSeconds.load());
    }

    // ------------------------------------------------------- 16 bars, 3/4
    {
        Rig r;
        r.set ("loopBars", 5.0f); // 16 Takte
        r.host.num = 3; r.host.bpm = 140.0;
        r.input = [] (double) { return 0.1f; };
        r.proc.sendCommand (LoopTapeProcessor::CmdRecord); r.block();
        r.run (16 * 3 * 60.0 / 140.0 + 0.2);
        const double want = 16 * 3 * 60.0 / 140.0;
        CHECK (r.state() == (int) S::Playing && std::abs (r.proc.info.loopSeconds.load() - want) < 1e-4,
               "16 bars of 3/4 at 140 BPM = %.3f s (got %.3f)", want, r.proc.info.loopSeconds.load());
    }

    // ------------------------------------------------------- HOST off: free running, fixed bars still work
    {
        Rig r;
        r.set ("hostSync", 0.0f);
        r.set ("loopBars", 2.0f); // 2 Takte
        r.host.ppq = 1.7;
        r.input = [] (double) { return 0.1f; };
        r.proc.sendCommand (LoopTapeProcessor::CmdRecord); r.block();
        CHECK (r.state() == (int) S::Recording, "HOST off: REC starts immediately");
        r.run (4.2);
        CHECK (r.state() == (int) S::Playing && std::abs (r.proc.info.loopSeconds.load() - 4.0) < 1e-6, "HOST off: 2 bars at host tempo");
        r.host.playing = false; r.run (0.1);
        float mx = 0; for (float x : r.outL) mx = std::max (mx, std::abs (x));
        CHECK (mx > 0.05f, "HOST off: loop keeps playing when Ableton stops");
    }

    std::printf ("\n%s (%d failures)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}
