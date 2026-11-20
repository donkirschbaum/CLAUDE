// Offline test of the DSP core (no JUCE, no host). Build with
//   cmake -B build -DLOOPTAPE_BUILD_TESTS=ON && cmake --build build --target looptape_test
#include "Looper.h"
#include "TapeDelay.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace lt;

static int failures = 0;
#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (! (cond)) { std::printf ("FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); ++failures; } \
        else std::printf ("ok:   %s\n", msg);                                  \
    } while (0)

constexpr double SR = 48000.0;

struct Rig
{
    Looper lp;
    Rig() { lp.prepare (SR); lp.allocateExtraLayers(); }

    // run n samples; input generator f(i); returns loop output
    template <typename F>
    std::vector<float> run (int n, F f, std::vector<float>* outR = nullptr)
    {
        std::vector<float> out ((size_t) n);
        if (outR) outR->resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            float l, r;
            lp.read (l, r);
            out[(size_t) i] = l;
            if (outR) (*outR)[(size_t) i] = r;
            const float x = f (i);
            lp.writeAndAdvance (x, -x);
            if (i % 512 == 0) lp.allocateExtraLayers(); // "message thread"
        }
        return out;
    }
};

static float rms (const std::vector<float>& v, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i];
    return (float) std::sqrt (s / (double) (b - a));
}

static float sine (int i, float hz) { return 0.5f * std::sin (2.0f * 3.14159265f * hz * (float) i / (float) SR); }

int main()
{
    const int sec = (int) SR;

    // ------------------------------------------------------------ looper basics
    {
        Rig rig;
        auto& lp = rig.lp;
        lp.pressRecord();
        CHECK (lp.getState() == Looper::State::Recording, "rec starts recording");
        rig.run (sec, [] (int i) { return sine (i, 220.0f); });
        lp.pressRecord();
        CHECK (lp.getState() == Looper::State::Playing, "second press -> playing");
        CHECK (lp.getLoopLength() == sec, "loop length = recorded length");

        auto out = rig.run (sec, [] (int) { return 0.0f; });
        // compare with source (skip fades)
        double err = 0; for (int i = 1000; i < sec - 1000; ++i) err = std::max (err, (double) std::abs (out[(size_t) i] - sine (i, 220.0f)));
        CHECK (err < 1e-4, "playback reproduces base layer");

        // overdub layer 2 for exactly one loop
        lp.pressRecord();
        CHECK (lp.getState() == Looper::State::Overdubbing, "dub press -> overdubbing");
        rig.run (sec, [] (int i) { return sine (i, 330.0f); });
        lp.pressRecord();
        CHECK (lp.getNumLayers() == 2, "two layers after overdub");
        rig.run (2000, [] (int) { return 0.0f; });
        auto o2 = rig.run (sec - 2000, [] (int) { return 0.0f; });
        // position now: 2000..sec
        double e2 = 0; for (int i = 2000; i < sec - 2000; ++i)
            e2 = std::max (e2, (double) std::abs (o2[(size_t) (i - 2000)] - (sine (i, 220.0f) + sine (i, 330.0f))));
        CHECK (e2 < 2e-3, "layer 1 + layer 2 summed correctly");

        lp.pressUndo();
        CHECK (lp.getNumLayers() == 1 && lp.getNumStored() == 2, "undo removes top layer, redo kept");
        auto o3 = rig.run (sec, [] (int) { return 0.0f; });
        double e3 = 0; for (int i = 2000; i < sec - 2000; ++i) e3 = std::max (e3, (double) std::abs (o3[(size_t) i] - sine (i, 220.0f)));
        CHECK (e3 < 2e-3, "after undo only base layer plays");

        lp.pressRedo();
        CHECK (lp.getNumLayers() == 2, "redo brings layer back");

        // stop / play
        lp.pressPlayStop();
        rig.run (2000, [] (int) { return 0.0f; });
        auto s1 = rig.run (4000, [] (int) { return 0.0f; });
        CHECK (rms (s1, 0, s1.size()) < 1e-6, "stopped = silence");
        lp.pressPlayStop();
        CHECK (lp.getState() == Looper::State::Playing && lp.getPosition() == 0.0, "play restarts at loop start");

        // new overdub kills redo history
        lp.pressUndo();
        lp.pressRecord();
        rig.run (sec / 2, [] (int i) { return sine (i, 500.0f); });
        lp.pressRecord();
        CHECK (lp.getNumLayers() == 2 && lp.getNumStored() == 2, "new take replaces redo slot");
        // half-loop take: second half of loop must contain no 500 Hz (stale data from old layer 2)
        rig.run (2000, [] (int) { return 0.0f; });
        auto o4 = rig.run (sec / 2 - 4000, [] (int) { return 0.0f; });
        double e4 = 0; for (size_t i = 0; i < o4.size(); ++i)
            e4 = std::max (e4, (double) std::abs (o4[i] - sine ((int) (sec / 2 + 2000 + i), 220.0f)));
        CHECK (e4 < 2e-3, "unwritten part of a reused layer is silent (no stale audio)");

        // clear
        lp.pressClear();
        rig.run (2000, [] (int) { return 0.0f; });
        CHECK (lp.getState() == Looper::State::Empty, "clear -> empty");
    }

    // ------------------------------------------------------------ undo while dubbing
    {
        Rig rig; auto& lp = rig.lp;
        lp.pressRecord(); rig.run (sec / 2, [] (int i) { return sine (i, 220.0f); }); lp.pressRecord();
        rig.run (100, [] (int) { return 0.f; });
        lp.pressRecord(); rig.run (1000, [] (int i) { return sine (i, 700.0f); });
        lp.pressUndo();
        CHECK (lp.getNumLayers() == 1 && lp.getState() == Looper::State::Playing, "undo during overdub discards the take");
    }

    // ------------------------------------------------------------ pending dub until layers allocated
    {
        Looper lp; lp.prepare (SR); lp.allocateExtraLayers();
        lp.pressRecord();
        for (int i = 0; i < sec / 4; ++i) { float l, r; lp.read (l, r); lp.writeAndAdvance (0.1f, 0.1f); }
        lp.pressRecord();
        lp.pressRecord(); // dub immediately, before message thread allocated
        CHECK (lp.isPendingDub(), "dub waits for layer allocation");
        lp.allocateExtraLayers();
        float l, r; lp.read (l, r); lp.writeAndAdvance (0.1f, 0.1f);
        CHECK (! lp.isPendingDub() && lp.getState() == Looper::State::Overdubbing && lp.getNumLayers() == 2,
               "dub starts once allocated");
    }

    // ------------------------------------------------------------ quantize
    {
        Rig rig; auto& lp = rig.lp;
        lp.setQuantizeUnit (SR * 0.5);
        lp.pressRecord(); rig.run ((int) (SR * 1.2), [] (int) { return 0.1f; }); lp.pressRecord();
        CHECK (lp.getLoopLength() == (int64_t) SR, "quantize rounds 1.2 s down to 1.0 s");
        lp.pressClear(); rig.run (2000, [] (int) { return 0.f; });

        lp.pressRecord(); rig.run ((int) (SR * 1.3), [] (int) { return 0.1f; }); lp.pressRecord();
        CHECK (lp.getState() == Looper::State::Recording, "quantize keeps recording to next grid point");
        rig.run ((int) (SR * 0.25), [] (int) { return 0.1f; });
        CHECK (lp.getLoopLength() == (int64_t) (SR * 1.5) && lp.getState() == Looper::State::Playing, "loop closes at 1.5 s");
    }

    // ------------------------------------------------------------ reverse + half speed
    {
        Rig rig; auto& lp = rig.lp;
        lp.pressRecord(); rig.run (sec, [] (int i) { return (float) i / (float) SR; }); lp.pressRecord(); // ramp 0..1
        lp.setReverse (true);
        rig.run (10, [] (int) { return 0.f; });
        auto o = rig.run (sec / 2, [] (int) { return 0.f; });
        CHECK (o[1000] > o[2000] && o[2000] > o[3000], "reverse plays backwards");
        lp.setReverse (false); lp.setHalfSpeed (true);
        const double p0 = lp.getPosition();
        rig.run (1000, [] (int) { return 0.f; });
        CHECK (std::abs ((lp.getPosition() - p0) - 500.0) < 1e-6, "half speed advances 0.5 sample per sample");
        // overdub at half speed must not crash / blow up
        lp.pressRecord(); auto oh = rig.run (sec * 3, [] (int i) { return sine (i, 300.f); }); lp.pressRecord();
        bool finite = true; for (float v : oh) finite &= std::isfinite (v) && std::abs (v) < 4.0f;
        CHECK (finite, "half speed overdub stays bounded");
    }

    // ------------------------------------------------------------ max length
    {
        Rig rig; auto& lp = rig.lp;
        lp.pressRecord();
        rig.run ((int) (SR * Looper::kMaxSeconds) + 10, [] (int) { return 0.01f; });
        CHECK (lp.getState() == Looper::State::Playing && lp.getLoopLength() == (int64_t) (SR * Looper::kMaxSeconds),
               "recording auto-closes at max length");
    }

    // ------------------------------------------------------------ tape delay
    {
        TapeDelay d; d.prepare (SR);
        TapeDelay::Params p; p.timeMs = 250; p.feedback = 0.5f; p.wow = 0; p.flutter = 0; p.saturation = 0; p.tone = 1; p.lowCut = 0;
        d.setParams (p);
        const int n = sec * 2;
        std::vector<float> L (n, 0.f), R (n, 0.f), wl (n), wr (n);
        for (int i = 0; i < 200; ++i) L[(size_t) i] = R[(size_t) i] = sine (i, 1000.f) * 1.6f;
        d.process (L.data(), R.data(), wl.data(), wr.data(), n);
        const size_t t = (size_t) (0.25 * SR);
        CHECK (rms (wl, 0, t - 50) < 1e-6, "no output before delay time");
        const float e1 = rms (wl, t, t + 200), e2 = rms (wl, 2 * t, 2 * t + 200);
        CHECK (e1 > 0.2f, "first echo at delay time");
        CHECK (e2 < e1 * 0.65f && e2 > e1 * 0.3f, "second echo ~ feedback * first");

        // self oscillation stays bounded
        TapeDelay d2; d2.prepare (SR);
        p.feedback = 1.1f; p.saturation = 1.0f; p.wow = 1; p.flutter = 1; p.hiss = 1; p.timeMs = 30; d2.setParams (p);
        std::vector<float> in (sec * 10, 0.f); for (int i = 0; i < 500; ++i) in[(size_t) i] = 0.9f;
        std::vector<float> ol (in.size()), orr (in.size());
        d2.process (in.data(), in.data(), ol.data(), orr.data(), (int) in.size());
        float mx = 0; bool fin = true; for (float v : ol) { mx = std::max (mx, std::abs (v)); fin &= std::isfinite (v); }
        std::printf ("      max level at 110%% feedback: %.3f\n", mx);
        CHECK (fin && mx < 2.0f, "110% feedback self-oscillates but stays bounded");

        // time change glides (no discontinuity jump > small step)
        TapeDelay d3; d3.prepare (SR); p = {}; p.wow = 0; p.flutter = 0; p.timeMs = 300; d3.setParams (p);
        std::vector<float> s (sec * 2); for (int i = 0; i < (int) s.size(); ++i) s[(size_t) i] = sine (i, 200.f);
        std::vector<float> a (s.size()), b (s.size());
        d3.process (s.data(), s.data(), a.data(), b.data(), sec);
        p.timeMs = 900; d3.setParams (p);
        d3.process (s.data() + sec, s.data() + sec, a.data() + sec, b.data() + sec, sec);
        float maxStep = 0; for (int i = sec / 2; i < 2 * sec - 1; ++i) maxStep = std::max (maxStep, std::abs (a[(size_t) i + 1] - a[(size_t) i]));
        CHECK (maxStep < 0.05f, "delay time change glides without clicks");
    }

    std::printf ("\n%s (%d failures)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}
