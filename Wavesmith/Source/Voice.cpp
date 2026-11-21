#include "Voice.h"

namespace ws
{

void Voice::prepare (double sr, uint32_t seed)
{
    sampleRate = (float) sr;
    ampEnv.setSampleRate (sr);
    modEnv.setSampleRate (sr);
    rng.seed (seed);
    kill();
}

void Voice::resetOscillators (const SynthSettings& s)
{
    for (int o = 0; o < 2; ++o)
        for (int u = 0; u < maxUnison; ++u)
            phase[o][u] = s.osc[o].randPhase ? rng.uni() : s.osc[o].phase;
    subPhase = 0.0f;
    noiseLp = 0.0f;
    for (auto& ch : svf)
        for (auto& st : ch)
            st.reset();
}

void Voice::start (int n, float vel, float glideFromNote, const SynthSettings& s)
{
    const bool wasActive = active;
    note = n;
    velocity = vel;
    noteRandom = rng.bi();
    targetPitch = (float) n;
    currentPitch = (s.glide > 0.0f && glideFromNote >= 0.0f) ? glideFromNote : (float) n;
    released = false;
    sustained = false;
    active = true;
    age = ++ageCounter;

    if (! wasActive)
    {
        resetOscillators (s);
        ampEnv.reset();
        modEnv.reset();
    }

    for (int l = 0; l < 2; ++l)
        if (s.lfo[l].retrig)
        {
            lfoPhase[l] = 0.0f;
            lfoPrev[l] = lfoHeld[l];
            lfoHeld[l] = rng.bi();
        }

    ampEnv.noteOn();
    modEnv.noteOn();
}

void Voice::legatoTo (int n)
{
    note = n;
    targetPitch = (float) n;
    released = false;
    sustained = false;
}

void Voice::release()
{
    released = true;
    ampEnv.noteOff();
    modEnv.noteOff();
}

void Voice::kill()
{
    active = false;
    released = false;
    sustained = false;
    ampEnv.reset();
    modEnv.reset();
}

void Voice::render (float* outL, float* outR, int n, const SynthSettings& s, const GlobalMod& g)
{
    if (! active)
        return;

    // ---- control rate -------------------------------------------------------
    ampEnv.setParams (s.ampA, s.ampD, s.ampS, s.ampR);
    modEnv.setParams (s.envA, s.envD, s.envS, s.envR);

    if (s.glide > 0.0f)
    {
        const float coef = std::exp (-(float) n / (s.glide * 0.25f * sampleRate + 1.0f));
        currentPitch = targetPitch + (currentPitch - targetPitch) * coef;
    }
    else
        currentPitch = targetPitch;

    // Mod sources
    float src[numModSources] {};
    src[srcModEnv]     = modEnv.getLevel();
    src[srcStepSeq]    = g.seqValue;
    src[srcVelocity]   = velocity;
    src[srcModWheel]   = g.modWheel;
    src[srcKeyTrack]   = ((float) note - 60.0f) / 48.0f;
    src[srcRandom]     = noteRandom;
    src[srcAftertouch] = g.aftertouch;

    float lfoRateMod[2] { 0, 0 };
    for (const auto& m : s.matrix)
    {
        if (m.dst == dstLfo1Rate) lfoRateMod[0] += m.amt * src[m.src];
        if (m.dst == dstLfo2Rate) lfoRateMod[1] += m.amt * src[m.src];
    }

    for (int l = 0; l < 2; ++l)
    {
        if (s.lfo[l].retrig)
        {
            const float rate = s.lfo[l].rateHz * std::exp2 (lfoRateMod[l] * 4.0f);
            lfoPhase[l] += rate * (float) n / sampleRate;
            if (lfoPhase[l] >= 1.0f)
            {
                lfoPhase[l] -= std::floor (lfoPhase[l]);
                lfoPrev[l] = lfoHeld[l];
                lfoHeld[l] = rng.bi();
            }
            src[srcLfo1 + l] = lfoShape (s.lfo[l].shape, lfoPhase[l], lfoHeld[l], lfoPrev[l]);
        }
        else
            src[srcLfo1 + l] = lfoShape (s.lfo[l].shape, g.lfoPhase[l], g.lfoHeld[l], g.lfoPrevHeld[l]);
    }

    float d[numModDests] {};
    float ampMult = 1.0f;
    for (const auto& m : s.matrix)
    {
        if (m.src == srcNone || m.dst == dstNone || m.amt == 0.0f)
            continue;
        const float v = src[m.src];
        if (m.dst == dstAmp)
            ampMult *= m.amt >= 0.0f ? 1.0f + m.amt * (v - 1.0f) : 1.0f + m.amt * v;
        else
            d[m.dst] += m.amt * v;
    }
    ampMult = clampf (ampMult, 0.0f, 2.0f);

    const float basePitch = currentPitch + g.pitchBend * (float) s.bendRange + d[dstPitch] * 24.0f;

    // Oscillator setup
    struct OscRun
    {
        bool run = false, audible = false;
        int uni = 1, level = 0, table = 0;
        float pos = 0, gain = 0;
        float inc[maxUnison] {}, gl[maxUnison] {}, gr[maxUnison] {};
    } osc[2];

    const int posDst[2]   { dstOscAPos, dstOscBPos };
    const int pitchDst[2] { dstOscAPitch, dstOscBPitch };
    const int levelDst[2] { dstOscALevel, dstOscBLevel };
    const int detDst[2]   { dstOscADetune, dstOscBDetune };
    const float fmAmt = clamp01 (s.fm + d[dstFm]);

    for (int o = 0; o < 2; ++o)
    {
        const auto& os = s.osc[o];
        auto& r = osc[o];
        r.audible = os.on;
        r.run = os.on || (o == 1 && fmAmt > 0.0f);
        if (! r.run) continue;

        r.table = juce::jlimit (0, WavetableBank::numTables - 1, os.table);
        r.pos = clamp01 (os.pos + d[posDst[o]]);
        r.uni = juce::jlimit (1, maxUnison, os.uni);
        r.gain = clamp01 (os.level + d[levelDst[o]]) / std::sqrt ((float) r.uni);

        const float pitch = basePitch + (float) (os.oct * 12 + os.semi) + os.fine * 0.01f + d[pitchDst[o]] * 24.0f;
        const float hz = midiToHz (pitch);
        const float det = clamp01 (os.detune + d[detDst[o]]);
        float maxInc = 0.0f;

        for (int u = 0; u < r.uni; ++u)
        {
            const float off = r.uni == 1 ? 0.0f : (2.0f * (float) u / (float) (r.uni - 1) - 1.0f);
            // slightly uneven spacing sounds less static
            const float shaped = off * (0.85f + 0.15f * std::abs (off));
            r.inc[u] = hz * semitoneRatio (shaped * det * det * 0.9f + shaped * det * 0.15f) / sampleRate;
            maxInc = std::max (maxInc, r.inc[u]);
            const float pan = clampf (os.pan + d[dstPan] + off * os.spread, -1.0f, 1.0f);
            const float angle = (pan + 1.0f) * 0.25f * kPi;
            r.gl[u] = std::cos (angle) * 1.41421356f;
            r.gr[u] = std::sin (angle) * 1.41421356f;
        }
        r.level = WavetableBank::levelForIncrement (maxInc * 1.06f);
    }

    // Sub & noise
    const float subLevel = clamp01 (s.subLevel + d[dstSubLevel]);
    const float noiseLevel = clamp01 (s.noiseLevel + d[dstNoiseLevel]);
    const float subInc = midiToHz (basePitch - (s.subOct == 0 ? 12.0f : 24.0f)) / sampleRate;
    const float noiseCoef = 0.02f + 0.9f * s.noiseColor * s.noiseColor;

    // Filter setup
    SvfCoeffs c1, c2;
    float drive = 0.0f;
    if (s.fltOn)
    {
        const float octaves = s.fltEnv * modEnv.getLevel() * 7.0f
                            + s.fltKey * ((float) note - 60.0f) / 12.0f
                            + s.fltVel * (velocity - 0.5f) * 4.0f
                            + d[dstCutoff] * 7.0f;
        const float cutoff = clampf (s.cutoff * std::exp2 (octaves), 20.0f, 20000.0f);
        const float reso = clamp01 (s.reso + d[dstReso]);
        drive = clamp01 (s.drive + d[dstDrive]);
        if (s.fltType == 1) // 24 dB: two stages, resonance on the second
        {
            c1.set (cutoff, 1.4142f, sampleRate);
            c2.set (cutoff, 2.0f - 1.97f * reso, sampleRate);
        }
        else
            c1.set (cutoff, 2.0f - 1.97f * reso, sampleRate);
    }
    const float driveGain = 1.0f + drive * 6.0f;
    const float driveComp = 1.0f / std::sqrt (driveGain);

    const float velGain = 1.0f - s.ampVel * (1.0f - velocity);

    // ---- audio rate ---------------------------------------------------------------
    for (int i = 0; i < n; ++i)
    {
        float l = 0.0f, r = 0.0f;

        // Osc B first (it modulates A)
        float bMono = 0.0f;
        if (osc[1].run)
        {
            auto& o = osc[1];
            float bl = 0.0f, br = 0.0f;
            for (int u = 0; u < o.uni; ++u)
            {
                const float v = bank.read (o.table, o.pos, o.level, phase[1][u]);
                phase[1][u] += o.inc[u];
                if (phase[1][u] >= 1.0f) phase[1][u] -= 1.0f;
                bl += v * o.gl[u];
                br += v * o.gr[u];
                bMono += v;
            }
            bMono /= (float) o.uni;
            if (o.audible) { l += bl * o.gain; r += br * o.gain; }
        }

        if (osc[0].run)
        {
            auto& o = osc[0];
            const float pm = fmAmt * fmAmt * 1.5f * bMono;
            for (int u = 0; u < o.uni; ++u)
            {
                float ph = phase[0][u] + pm;
                ph -= std::floor (ph);
                const float v = bank.read (o.table, o.pos, o.level, ph);
                phase[0][u] += o.inc[u];
                if (phase[0][u] >= 1.0f) phase[0][u] -= 1.0f;
                l += v * o.gl[u] * o.gain;
                r += v * o.gr[u] * o.gain;
            }
        }

        if (subLevel > 0.0f)
        {
            float v;
            if (s.subShape == 0)      v = std::sin (kTwoPi * subPhase);
            else if (s.subShape == 1) v = subPhase < 0.5f ? (4.0f * subPhase - 1.0f) : (3.0f - 4.0f * subPhase);
            else                      v = subPhase < 0.5f ? 0.8f : -0.8f;
            subPhase += subInc;
            if (subPhase >= 1.0f) subPhase -= 1.0f;
            l += v * subLevel;
            r += v * subLevel;
        }

        if (noiseLevel > 0.0f)
        {
            const float w = rng.bi();
            noiseLp += noiseCoef * (w - noiseLp);
            const float v = noiseLp * noiseLevel * (1.5f - s.noiseColor);
            l += v;
            r += v;
        }

        if (s.fltOn)
        {
            if (drive > 0.0f)
            {
                l = fastTanh (l * driveGain) * driveComp;
                r = fastTanh (r * driveGain) * driveComp;
            }
            float* ch[2] { &l, &r };
            for (int c = 0; c < 2; ++c)
            {
                float lo, bp, hp;
                svf[c][0].process (*ch[c], c1, lo, bp, hp);
                float y;
                switch (s.fltType)
                {
                    case 0:  y = lo; break;
                    case 1:  { float lo2, bp2, hp2; svf[c][1].process (lo, c2, lo2, bp2, hp2); y = lo2; } break;
                    case 2:  y = bp * 1.5f; break;
                    case 3:  y = hp; break;
                    default: y = lo + hp; break;
                }
                *ch[c] = y;
            }
        }

        const float amp = ampEnv.process() * velGain * ampMult;
        modEnv.process();

        outL[i] += l * amp;
        outR[i] += r * amp;
    }

    if (! ampEnv.isActive())
        active = false;
}

} // namespace ws
