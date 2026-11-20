#include "Engine.h"
#include <cmath>
#include <numeric>

namespace shaker
{
static inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

void Biquad::calc (int type, double fs, double f, double q, double db)
{
    f = juce::jlimit (10.0, fs * 0.45, f);
    q = juce::jmax (0.1, q);
    const double w = juce::MathConstants<double>::twoPi * f / fs;
    const double cw = std::cos (w), sw = std::sin (w);
    const double alpha = sw / (2.0 * q);
    double B0, B1, B2, A0, A1, A2;
    if (type == 0) { B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = (1 - cw) / 2; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; }
    else if (type == 1) { B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = (1 + cw) / 2; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; }
    else if (type == 2)
    {
        const double A = std::pow (10.0, db / 40.0);
        B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A; A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A;
    }
    else
    {
        const double A = std::pow (10.0, db / 40.0);
        const double sq = 2 * std::sqrt (A) * (sw / 2.0) * std::sqrt (2.0);
        B0 = A * ((A + 1) + (A - 1) * cw + sq);
        B1 = -2 * A * ((A - 1) + (A + 1) * cw);
        B2 = A * ((A + 1) + (A - 1) * cw - sq);
        A0 = (A + 1) - (A - 1) * cw + sq;
        A1 = 2 * ((A - 1) - (A + 1) * cw);
        A2 = (A + 1) - (A - 1) * cw - sq;
    }
    b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
}

Engine::Engine (SampleLibrary& l) : lib (l)
{
    for (auto& s : shared)
        for (auto& v : s.pat)
            v.store (0.0f);
}

void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    for (auto& t : tr)
    {
        t.buf.setSize (2, maxBlock, false, true, true);
        for (auto& v : t.voices) v.active = false;
        t.hp.reset(); t.lp.reset(); t.body.reset(); t.air.reset();
        t.stripInit = false;
        t.lastGs = -1; t.lastSb = -1;
    }
    revBuf.setSize (2, maxBlock, false, true, true);
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    revHp.setHighpass (sr, 350.0, 0.7);
    lastRoomSize = -1;
    wasPlaying = false;
}

float Engine::gauss()
{
    return (rng.nextFloat() + rng.nextFloat() + rng.nextFloat() - 1.5f) / 1.5f;
}

void Engine::updateStrip (int ti, const TrackParams& p)
{
    auto& t = tr[ti];
    if (t.stripInit && p.lowcut == t.last.lowcut && p.highcut == t.last.highcut && p.reso == t.last.reso
        && p.bodyF == t.last.bodyF && p.body == t.last.body && p.air == t.last.air)
        return;
    t.hp.setHighpass (sr, p.lowcut, 0.6);
    t.hp.bypass = p.lowcut <= 21.0f;
    t.lp.setLowpass (sr, p.highcut, p.reso);
    t.lp.bypass = p.highcut >= 19900.0f && std::abs (p.reso - 0.707f) < 0.01f;
    t.body.setPeak (sr, p.bodyF, 1.1, p.body);
    t.body.bypass = std::abs (p.body) < 0.05f;
    t.air.setHighShelf (sr, 8000.0, p.air);
    t.air.bypass = std::abs (p.air) < 0.05f;
    t.last = p;
    t.stripInit = true;
}

void Engine::process (juce::AudioBuffer<float>& out, const Transport& trIn, const GlobalParams& g, const TrackParams* tp)
{
    const int n = out.getNumSamples();
    out.clear();
    revBuf.clear (0, n);
    for (auto& t : tr) t.buf.clear (0, n);

    // Transport: Host oder interner Takt
    const Transport& t = trIn;
    anySolo = false;
    for (int i = 0; i < kTracks; ++i) anySolo = anySolo || tp[i].solo;
    const double spb = sr * 60.0 / juce::jmax (20.0, t.bpm);
    const double b0 = t.ppq, b1 = t.ppq + n / spb;
    const bool jumped = ! wasPlaying || std::abs (b0 - prevEnd) > 0.01;

    // Anspielen aus der Oberfläche
    for (int ti = 0; ti < kTracks; ++ti)
    {
        const int a = shared[ti].audition.exchange (0);
        if (a != 0)
            startVoice (ti, 0, 0.85f, a == 2 ? 60.0 : 0.35, tp[ti], 0.0f, true);
    }

    if (t.playing)
        for (int ti = 0; ti < kTracks; ++ti)
            scheduleTrack (ti, tp[ti], g, b0, b1, spb, jumped, n);
    else
        for (auto& s : shared) s.curStep.store (-1);

    wasPlaying = t.playing;
    prevEnd = b1;

    // Stimmen rendern und Kanalzug pro Spur
    float* oL = out.getWritePointer (0);
    float* oR = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    float* rL = revBuf.getWritePointer (0);
    float* rR = revBuf.getWritePointer (1);

    for (int ti = 0; ti < kTracks; ++ti)
    {
        auto& rt = tr[ti];
        const auto& p = tp[ti];
        float* L = rt.buf.getWritePointer (0);
        float* R = rt.buf.getWritePointer (1);
        bool any = false;
        for (auto& v : rt.voices)
            if (v.active) { renderVoice (v, L, R, n); any = true; }
        if (! any && rt.compEnv < 1e-5f)
            continue;

        updateStrip (ti, p);
        const float k = 1.0f + p.drive * 7.0f, tk = std::tanh (k);
        const bool drv = p.drive > 0.001f;
        const float thrDb = -p.comp * 32.0f, ratio = 1.0f + p.comp * 7.0f;
        const float aAtt = std::exp (-1.0f / (float) (sr * 0.003)), aRel = std::exp (-1.0f / (float) (sr * 0.08));
        const float a = (1.0f + p.width) * 0.5f, b = (1.0f - p.width) * 0.5f;
        const float makeup = p.vol * (1.0f + p.comp * 1.4f) / (1.0f + p.drive * 1.6f);
        const float pl = p.pan <= 0 ? 1.0f : 1.0f - p.pan, pr = p.pan >= 0 ? 1.0f : 1.0f + p.pan;

        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            if (! rt.hp.bypass)   { l = rt.hp.process (l, 0);   r = rt.hp.process (r, 1); }
            if (! rt.lp.bypass)   { l = rt.lp.process (l, 0);   r = rt.lp.process (r, 1); }
            if (! rt.body.bypass) { l = rt.body.process (l, 0); r = rt.body.process (r, 1); }
            if (! rt.air.bypass)  { l = rt.air.process (l, 0);  r = rt.air.process (r, 1); }
            if (drv) { l = std::tanh (k * l) / tk; r = std::tanh (k * r) / tk; }
            if (p.comp > 0.001f)
            {
                const float lvl = juce::jmax (std::abs (l), std::abs (r));
                rt.compEnv = lvl > rt.compEnv ? aAtt * rt.compEnv + (1 - aAtt) * lvl : aRel * rt.compEnv + (1 - aRel) * lvl;
                const float envDb = 20.0f * std::log10 (rt.compEnv + 1e-9f);
                if (envDb > thrDb)
                {
                    const float gr = dbToGain ((thrDb - envDb) * (1.0f - 1.0f / ratio));
                    l *= gr; r *= gr;
                }
            }
            else rt.compEnv *= aRel;
            const float wl = a * l + b * r, wr = b * l + a * r;
            const float fl = wl * makeup * pl, fr = wr * makeup * pr;
            oL[i] += fl;
            if (oR) oR[i] += fr;
            rL[i] += fl * p.rev;
            rR[i] += fr * p.rev;
        }
    }

    // Hall
    if (g.room > 0.001f)
    {
        if (std::abs (g.roomSize - lastRoomSize) > 0.001f)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = 0.25f + g.roomSize * 0.7f;
            rp.damping = 0.45f;
            rp.wetLevel = 1.0f;
            rp.dryLevel = 0.0f;
            rp.width = 1.0f;
            reverb.setParameters (rp);
            lastRoomSize = g.roomSize;
        }
        for (int i = 0; i < n; ++i) { rL[i] = revHp.process (rL[i], 0); rR[i] = revHp.process (rR[i], 1); }
        reverb.processStereo (rL, rR, n);
        for (int i = 0; i < n; ++i)
        {
            oL[i] += rL[i] * g.room * 0.6f;
            if (oR) oR[i] += rR[i] * g.room * 0.6f;
        }
    }

    // Master + Limiter
    const float relL = std::exp (-1.0f / (float) (sr * 0.06));
    const float ceil = dbToGain (-1.0f);
    for (int i = 0; i < n; ++i)
    {
        float l = oL[i] * g.master * 2.0f, r = oR ? oR[i] * g.master * 2.0f : l;
        const float pk = juce::jmax (std::abs (l), std::abs (r));
        const float need = pk > ceil ? ceil / pk : 1.0f;
        limEnv = need < limEnv ? need : relL * limEnv + (1 - relL) * need;
        l *= limEnv; r *= limEnv;
        oL[i] = juce::jlimit (-1.0f, 1.0f, l);
        if (oR) oR[i] = juce::jlimit (-1.0f, 1.0f, r);
    }
}

void Engine::scheduleTrack (int ti, const TrackParams& p, const GlobalParams& g, double b0, double b1, double spb, bool jumped, int numSamples)
{
    auto& rt = tr[ti];
    const int steps = juce::jlimit (1, kMaxSteps, p.steps);
    const double cyc = cycleBeats (steps, p.mode, p.rate);
    const double sb = cyc / steps;
    if (jumped || std::abs (sb - rt.lastSb) > 1e-12)
    {
        rt.lastGs = (long) std::ceil (b0 / sb - 1e-9) - 1;
        rt.lastSb = sb;
    }
    const double swingAmt = (p.mode == Polymeter && steps % 2 == 0) ? g.swing * sb * 0.5 : 0.0;
    for (int guard = 0; guard < 512; ++guard)
    {
        const long gs = rt.lastGs + 1;
        const int idx = (int) (((gs % steps) + steps) % steps);
        const double tBeat = gs * sb + ((idx % 2 == 1) ? swingAmt : 0.0);
        if (tBeat >= b1)
            break;
        rt.lastGs = gs;
        if (tBeat < 0.0)
            continue;
        const int offset = juce::jlimit (0, numSamples - 1, (int) std::floor ((tBeat - b0) * spb));
        const long cycle = (long) std::floor ((double) gs / steps);
        playStep (ti, idx, offset, sb * spb / sr, p, g, cycle);
    }
}

void Engine::playStep (int ti, int idx, int offset, double stepSecIn, const TrackParams& p, const GlobalParams& g, long cycle)
{
    auto& rt = tr[ti];
    auto& sh = shared[ti];
    sh.curStep.store (idx);

    // Evolution: am Anfang jedes Durchlaufs einzelne Schritte verändern
    if (idx == 0 && cycle != rt.lastCycle)
    {
        rt.lastCycle = cycle;
        if (g.evolve > 0.0f)
        {
            bool ch = false;
            for (int i = 0; i < p.steps; ++i)
                if (rng.nextFloat() < g.evolve * 0.12f)
                {
                    const float v = sh.pat[(size_t) i].load();
                    float nv;
                    if (v > 0) nv = rng.nextFloat() < 0.4f ? 0.0f : juce::jlimit (0.12f, 1.0f, v + (rng.nextFloat() * 2 - 1) * 0.35f);
                    else nv = rng.nextFloat() < 0.5f ? 0.15f + rng.nextFloat() * 0.75f : 0.0f;
                    sh.pat[(size_t) i].store (nv);
                    ch = true;
                }
            if (ch) sh.patternChanged.store (true);
        }
    }

    rt.drift = (rt.drift + (rng.nextFloat() * 2 - 1) * 0.03f * g.drift) * 0.985f;

    if (p.mute || (anySolo && ! p.solo)) return;

    const float C = g.chaos;
    float v = sh.pat[(size_t) idx].load();
    if (v > 0) { if (rng.nextFloat() < p.rProb * C * 0.5f) return; }
    else
    {
        if (rng.nextFloat() < p.rProb * C * 0.12f) v = 0.12f + rng.nextFloat() * 0.2f;
        else return;
    }
    const float vel = juce::jlimit (0.05f, 1.0f, v * (1.0f + gauss() * p.rVel * C * 0.6f));
    const double stepSec = stepSecIn;
    const int delay = offset + (int) (std::abs (gauss()) * p.rTime * C * 0.022f * sr);

    if (rng.nextFloat() < p.rRatchet * C * 0.5f)
    {
        const int nr = rng.nextFloat() < 0.7f ? 2 : 3;
        const double sp = stepSec / nr;
        float vv = vel;
        for (int k = 0; k < nr; ++k)
        {
            startVoice (ti, delay + (int) (k * sp * sr), vv, sp, p, C, false);
            vv *= 0.72f;
        }
    }
    else
        startVoice (ti, delay, vel, stepSec, p, C, false);
}

void Engine::startVoice (int ti, int offset, float vel, double gateSec, const TrackParams& p, float C, bool clean)
{
    auto& rt = tr[ti];
    auto& sh = shared[ti];
    const SampleData* s = lib.get (sh.sample.load());
    if (s == nullptr || s->buf.getNumSamples() < 2)
        return;

    Voice* v = nullptr;
    for (auto& cand : rt.voices)
        if (! cand.active) { v = &cand; break; }
    if (v == nullptr)
    {
        v = &rt.voices[0];
        for (auto& cand : rt.voices)
            if (cand.age < v->age) v = &cand;
    }

    const double len = s->lengthSeconds();
    double s0 = juce::jlimit (0.0, len, (double) sh.start.load());
    double s1 = juce::jlimit (0.0, len, (double) sh.end.load());
    if (s1 - s0 < 0.003) s1 = juce::jmin (len, s0 + 0.003);
    if (! clean) s0 = juce::jmin (s0 + std::abs (gauss()) * p.rStart * C * 0.03, s1 - 0.003);

    const bool rev = ! clean && rng.nextFloat() < p.rRev * C * 0.5f;
    const float semis = std::round (p.tune) + p.fine / 100.0f + (clean ? 0.0f : gauss() * p.rPitch * C * 2.0f + rt.drift * 12.0f);
    const double rate = std::pow (2.0, semis / 12.0);

    v->s = s;
    v->regionStart = (int) (s0 * s->sr);
    v->regionEnd = juce::jmin (s->buf.getNumSamples() - 1, (int) (s1 * s->sr));
    v->reverse = rev;
    v->pos = rev ? (double) v->regionEnd : (double) v->regionStart;
    v->inc = rate * s->sr / sr;
    v->delay = offset;

    const float lm = clean ? 1.0f : juce::jmax (0.2f, 1.0f + gauss() * p.rLen * C * 0.6f);
    const float A = p.attack / 1000.0f, D = p.decay / 1000.0f * lm, R = juce::jmax (0.003f, p.release / 1000.0f * lm);
    v->peak = std::pow (vel, 1.3f) * dbToGain (p.gain + s->autoDb);
    v->sus = p.sustain * v->peak;
    v->stage = A > 0.0005f ? 0 : 1;
    v->level = A > 0.0005f ? 0.0f : v->peak;
    v->atkInc = A > 0.0005f ? v->peak / (float) (A * sr) : 0.0f;
    v->decCoef = std::exp (-1.0f / (float) (juce::jmax (0.001f, D / 4.0f) * sr));
    v->relCoef = std::exp (-1.0f / (float) (R / 4.0f * sr));
    v->gateLeft = juce::jmax (1, (int) (gateSec * sr));
    v->fadeLen = juce::jmax (1, (int) (0.004 * s->sr));

    float pan = 0.0f;
    if (! clean && p.rPan * C > 0.01f) pan = juce::jlimit (-1.0f, 1.0f, gauss() * p.rPan * C);
    v->gl = pan <= 0 ? 1.0f : 1.0f - pan;
    v->gr = pan >= 0 ? 1.0f : 1.0f + pan;
    v->age = ++voiceAge;
    v->active = true;

    sh.hits.fetch_add (1);
    if (clean)
    {
        sh.previewStartMs.store (juce::Time::getMillisecondCounterHiRes() + 1000.0 * offset / sr);
        sh.previewRate.store ((float) rate);
    }
}

static inline float hermite (const float* d, int n, double pos)
{
    const int i = (int) pos;
    const float f = (float) (pos - i);
    const float xm1 = d[juce::jlimit (0, n - 1, i - 1)];
    const float x0 = d[juce::jlimit (0, n - 1, i)];
    const float x1 = d[juce::jlimit (0, n - 1, i + 1)];
    const float x2 = d[juce::jlimit (0, n - 1, i + 2)];
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b = w + a;
    return ((((a * f) - b) * f + c) * f + x0);
}

void Engine::renderVoice (Voice& v, float* L, float* R, int n)
{
    const auto* s = v.s;
    const int len = s->buf.getNumSamples();
    const float* dL = s->buf.getReadPointer (0);
    const float* dR = s->buf.getNumChannels() > 1 ? s->buf.getReadPointer (1) : dL;
    int i = 0;
    if (v.delay > 0)
    {
        i = juce::jmin (n, v.delay);
        v.delay -= i;
        if (i >= n) return;
    }
    for (; i < n; ++i)
    {
        // Hüllkurve
        if (v.stage == 0)
        {
            v.level += v.atkInc;
            if (v.level >= v.peak) { v.level = v.peak; v.stage = 1; }
        }
        else if (v.stage == 1)
            v.level = v.sus + (v.level - v.sus) * v.decCoef;
        else
            v.level *= v.relCoef;

        if (v.stage < 2 && --v.gateLeft <= 0)
            v.stage = 2;

        // Ende des Ausschnitts erreicht?
        double remain = v.reverse ? v.pos - v.regionStart : v.regionEnd - v.pos;
        if (remain <= 0 || (v.stage == 2 && v.level < 1e-5f) || (v.stage == 1 && v.sus <= 0 && v.level < 1e-5f))
        {
            v.active = false;
            return;
        }
        float fade = remain < v.fadeLen ? (float) (remain / v.fadeLen) : 1.0f;
        const float g = v.level * fade;
        L[i] += hermite (dL, len, v.pos) * g * v.gl;
        R[i] += hermite (dR, len, v.pos) * g * v.gr;
        v.pos += v.reverse ? -v.inc : v.inc;
    }
}
} // namespace shaker
