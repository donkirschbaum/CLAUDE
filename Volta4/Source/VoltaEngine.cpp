// VOLTA·4 – Klangerzeugung. Portiert aus der Web-Audio-Version.
#include "VoltaEngine.h"
#include <algorithm>

namespace volta
{
static constexpr float kPi = 3.14159265358979f;
static inline float clamp01 (float v) { return std::min (1.f, std::max (0.f, v)); }
static inline float P2 (float x) { return std::exp2 (x); }

int srcFromId (const std::string& id)
{
    static const char* ids[S_NUM] = { "c1","c2","c4","c8","sA","sB","sC","sD","l1","l2","rnd","eK","eS","eB","eH","probO","divO" };
    for (int i = 0; i < S_NUM; ++i) if (id == ids[i]) return i;
    return -1;
}

int inFromId (const std::string& id)
{
    if (id == "probI") return I_PROB;
    if (id == "divI") return I_DIV;
    static const char* ch[kNumCh] = { "kick", "snare", "bass", "hats" };
    static const char* k[4] = { "trig", "pitch", "dec", "tone" };
    for (int c = 0; c < kNumCh; ++c)
        for (int j = 0; j < 4; ++j)
            if (id == std::string (ch[c]) + "." + k[j]) return inOf (c, j);
    return -1;
}

/* ---------- DSP ---------- */
void Biquad::set (Type t, float f, float q, float sr)
{
    f = std::min (std::max (f, 10.f), sr * 0.45f);
    const float w = 2.f * kPi * f / sr, cs = std::cos (w), sn = std::sin (w), al = sn / (2.f * q);
    float B0, B1, B2;
    switch (t)
    {
        case LP: B0 = (1 - cs) * .5f; B1 = 1 - cs; B2 = B0; break;
        case HP: B0 = (1 + cs) * .5f; B1 = -(1 + cs); B2 = B0; break;
        default: B0 = al; B1 = 0; B2 = -al; break;
    }
    const float a0 = 1 + al;
    b0 = B0 / a0; b1 = B1 / a0; b2 = B2 / a0; a1 = -2 * cs / a0; a2 = (1 - al) / a0;
}

void Env::start (float peak, float dec, float sr)
{
    peak = std::max (peak, 2e-4f); dec = std::max (dec, .004f);
    v = 1e-4f; aN = std::max (1, (int) (0.002f * sr));
    const int dN = std::max (1, (int) ((dec - 0.002f) * sr));
    aMul = std::pow (peak / 1e-4f, 1.f / (float) aN);
    dMul = std::pow (1e-4f / peak, 1.f / (float) dN);
    n = 0; total = aN + dN; active = true;
}

void Ramp::start (float from, float to, float time, float sr)
{
    v = std::max (from, 1e-3f); n = std::max (1, (int) (time * sr));
    mul = std::pow (std::max (to, 1e-3f) / v, 1.f / (float) n);
}

static inline float blep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}
static inline float sawB (float ph, float dt) { return 2.f * ph - 1.f - blep (ph, dt); }
static inline float sqrB (float ph, float dt)
{
    float ph2 = ph + .5f; if (ph2 >= 1.f) ph2 -= 1.f;
    return (ph < .5f ? 1.f : -1.f) + blep (ph, dt) - blep (ph2, dt);
}
static inline void adv (float& ph, float inc) { ph += inc; if (ph >= 1.f) ph -= 1.f; }

/* ---------- Engine ---------- */
void Engine::prepare (double sampleRate)
{
    sr = sampleRate;
    dl.assign ((size_t) (sr * 3.0) + 4, 0.f); dw = 0;
    dlp.set (Biquad::LP, 3200.f, .707f, (float) sr);
    smooth = 1.f - std::exp (-1.f / (0.02f * (float) sr));
    compA = 1.f - std::exp (-1.f / (0.003f * (float) sr));
    compR = std::exp (-1.f / (0.15f * (float) sr));
    for (auto& v : kick) v.on = false;
    for (auto& v : snare) v.on = false;
    for (auto& v : bass) v.on = false;
    for (auto& v : hats) v.on = false;
}

void Engine::resetSequencer()
{
    divCount = 0;
    for (auto& v : prevCV) v = 0;
}

float Engine::lfo (int i, double t) const
{
    const float r = i == 1 ? P.l1r : P.l2r; const int s = i == 1 ? P.l1s : P.l2s;
    const double hz = .05 * std::pow (200.0, (double) r);
    const float ph = (float) std::fmod (t * hz, 1.0);
    switch (s)
    {
        case 0: return std::sin (ph * 2.f * kPi);
        case 1: return 1.f - 4.f * std::abs (ph - .5f);
        case 2: return 2.f * ph - 1.f;
        default: return ph < .5f ? 1.f : -1.f;
    }
}

float Engine::envVal (int ch, int64_t at) const
{
    const double dt = (double) (at - lastTrig[ch]) / sr;
    return dt < 0 ? 0.f : (float) std::exp (-dt / (lastDec[ch] * .3));
}

void Engine::publishCV()
{
    const double t = (double) now / sr;
    cvShow[0].store (lfo (1, t)); cvShow[1].store (lfo (2, t)); cvShow[2].store (rndVal);
    for (int c = 0; c < kNumCh; ++c) cvShow[(size_t) (3 + c)].store (envVal (c, now));
}

/* ---------- Anschlag ---------- */
void Engine::trigger (int ch, float pc, float dc, float tc, float gain, int midiNote)
{
    const float fsr = (float) sr;
    const ChParams& Q = P.ch[ch];
    const int vi = rr[ch]; rr[ch] = (rr[ch] + 1) % kVoices;
    float dec = .3f;
    const float tone = clamp01 (Q.tone + tc * .5f);
    if (ch == 0)
    {
        auto& v = kick[vi];
        const float f = (35.f + Q.tune * 55.f) * P2 (pc);
        dec = (.08f + Q.decay * Q.decay * 1.6f) * P2 (dc * 1.5f);
        v.fr.start (f * (1.f + Q.snap * 7.f), f, .03f + .04f * (1.f - Q.snap), fsr);
        v.amp.start (1.f, dec, fsr); v.ph = 0;
        v.lp.reset(); v.lp.set (Biquad::LP, 150.f * P2 (tone * 7.f), .707f, fsr);
        v.hasClick = Q.snap > .05f;
        if (v.hasClick) { v.hp.reset(); v.hp.set (Biquad::HP, 2500.f, .707f, fsr); v.click.start (Q.snap * .35f, .012f, fsr); }
        v.gain = gain; v.on = true;
    }
    else if (ch == 1)
    {
        auto& v = snare[vi];
        dec = (.05f + Q.decay * Q.decay * .6f) * P2 (dc * 1.5f);
        const float f = (140.f + Q.tune * 220.f) * P2 (pc);
        v.fr.start (f * 1.6f, f, .02f, fsr); v.ph = 0;
        v.body.start ((1.f - Q.snap) * .8f + .1f, dec * .5f, fsr);
        v.bp.reset(); v.bp.set (Biquad::BP, 900.f * P2 (tone * 2.8f), .7f, fsr);
        v.nz.start (.2f + Q.snap * .8f, dec, fsr);
        v.gain = gain; v.on = true;
    }
    else if (ch == 2)
    {
        auto& v = bass[vi];
        int midi;
        if (midiNote >= 0) midi = std::min (std::max (midiNote - 24, 16), 72);
        else
        {
            static const int SC[5] = { 0, 3, 5, 7, 10 };
            const int root = 28 + (int) std::lround (Q.tune * 24.f);
            const int deg = (int) std::lround (pc * 5.f);
            const int oct = (int) std::floor (deg / 5.0), idx = ((deg % 5) + 5) % 5;
            midi = root + oct * 12 + SC[idx];
        }
        v.f = 440.f * P2 ((midi - 69) / 12.f);
        dec = (.06f + Q.decay * Q.decay * 1.2f) * P2 (dc * 1.5f);
        const float cut = 55.f * P2 (tone * 7.2f);
        v.q = std::pow (10.f, (1.f + Q.snap * 16.f) / 20.f);   // Web-Audio-Q in dB
        v.cut.start (std::min (cut * (1.f + Q.snap * 6.f), 18000.f), std::max (cut, 30.f), dec * .7f, fsr);
        v.lp.reset(); v.lp.set (Biquad::LP, v.cut.v, v.q, fsr); v.cnt = 0;
        v.ph1 = 0; v.ph2 = 0;
        v.amp.start (.6f, dec, fsr);
        v.gain = gain; v.on = true;
    }
    else
    {
        auto& v = hats[vi];
        static const float HR[6] = { 2.f, 3.f, 4.16f, 5.43f, 6.79f, 8.21f };
        dec = (.015f + Q.decay * Q.decay * .9f) * P2 (dc * 1.5f);
        const float base = 40.f * P2 (Q.tune * 1.5f + pc * .5f);
        for (int i = 0; i < 6; ++i) { v.fr[i] = base * HR[i] / fsr; v.ph[i] = 0; }
        v.mg = (1.f - Q.snap * .85f) * .3f; v.ng = Q.snap;
        v.bp.reset(); v.bp.set (Biquad::BP, 10000.f, .6f, fsr);
        v.hp.reset(); v.hp.set (Biquad::HP, 2500.f * P2 (tone * 2.f), .707f, fsr);
        v.amp.start (.55f, dec, fsr);
        v.gain = gain; v.on = true;
    }
    lastTrig[ch] = now; lastDec[ch] = dec;
    litMask.fetch_or (1u << (16 + ch));
}

/* ---------- Stimmen rendern ---------- */
float Engine::renderKick (KickVoice& v)
{
    v.ph += v.fr.next() / (float) sr; if (v.ph >= 1.f) v.ph -= 1.f;
    float s = std::sin (v.ph * 2.f * kPi) * v.amp.next();
    if (v.hasClick && v.click.active) s += v.hp.p (noise.next()) * v.click.next();
    s = v.lp.p (s);
    if (! v.amp.active) v.on = false;
    return s * v.gain;
}

float Engine::renderSnare (SnareVoice& v)
{
    const float f = v.fr.next() / (float) sr;
    v.ph += f; if (v.ph >= 1.f) v.ph -= 1.f;
    const float tri = 1.f - 4.f * std::abs (v.ph - .5f);
    float s = tri * v.body.next() + v.bp.p (noise.next()) * v.nz.next();
    if (! v.body.active && ! v.nz.active) v.on = false;
    return s * v.gain;
}

float Engine::renderBass (BassVoice& v)
{
    const float fsr = (float) sr;
    const float dt1 = v.f / fsr, dt2 = v.f * .5f / fsr;
    const float x = sawB (v.ph1, dt1) + .45f * sqrB (v.ph2, dt2);
    adv (v.ph1, dt1); adv (v.ph2, dt2);
    const float c = v.cut.next();
    if (++v.cnt >= 8) { v.cnt = 0; const float z1 = v.lp.z1, z2 = v.lp.z2; v.lp.set (Biquad::LP, c, v.q, fsr); v.lp.z1 = z1; v.lp.z2 = z2; }
    const float s = v.lp.p (x) * v.amp.next();
    if (! v.amp.active) v.on = false;
    return s * v.gain;
}

float Engine::renderHat (HatVoice& v)
{
    float m = 0;
    for (int i = 0; i < 6; ++i) { m += sqrB (v.ph[i], v.fr[i]); adv (v.ph[i], v.fr[i]); }
    const float x = v.bp.p (m * v.mg) + noise.next() * v.ng;
    const float s = v.hp.p (x) * v.amp.next();
    if (! v.amp.active) v.on = false;
    return s * v.gain;
}

void Engine::render (float* L, float* R, int n)
{
    static const int DDIV[5] = { 1, 2, 3, 4, 6 };
    const float fsr = (float) sr;
    const float tLevel[4] = { P.ch[0].level * P.ch[0].level, P.ch[1].level * P.ch[1].level, P.ch[2].level * P.ch[2].level, P.ch[3].level * P.ch[3].level };
    const float tMaster = P.vol * P.vol * 1.3f;
    const float targetDelay = std::min (2.9f, (float) (DDIV[(int) std::lround (P.dtime * 4.f)] * stepSec));
    const float dSmooth = 1.f - std::exp (-1.f / (0.05f * fsr));
    const float fb = P.dfb * .85f, wetG = P.dmix * 1.2f;
    const float kneeT = -12.f, ratio = 4.f, knee = 30.f, makeup = 1.877f;
    const int dlen = (int) dl.size();
    static const float shN = std::tanh (2.2f);

    for (int i = 0; i < n; ++i)
    {
        float ch[4] = { 0, 0, 0, 0 };
        for (auto& v : kick)  if (v.on) ch[0] += renderKick (v);
        for (auto& v : snare) if (v.on) ch[1] += renderSnare (v);
        for (auto& v : bass)  if (v.on) ch[2] += renderBass (v);
        for (auto& v : hats)  if (v.on) ch[3] += renderHat (v);
        ch[0] = std::tanh (std::min (1.f, std::max (-1.f, ch[0])) * 2.2f) / shN;   // Kick-Sättigung

        float dry = 0, send = 0;
        for (int c = 0; c < 4; ++c)
        {
            gLevel[c] += (tLevel[c] - gLevel[c]) * smooth;
            gFx[c] += (P.ch[c].fx - gFx[c]) * smooth;
            const float o = ch[c] * gLevel[c];
            dry += o; send += o * gFx[c];
        }
        // Echo
        dTime += (targetDelay - dTime) * dSmooth;
        float rp = (float) dw - dTime * fsr; while (rp < 0) rp += (float) dlen;
        const int i0 = (int) rp; const float fr = rp - (float) i0;
        const float y = dl[(size_t) i0] + (dl[(size_t) ((i0 + 1) % dlen)] - dl[(size_t) i0]) * fr;
        const float l = dlp.p (y);
        dl[(size_t) dw] = send + l * fb; dw = (dw + 1) % dlen;

        float x = dry + l * wetG;
        // Kompressor (wie Web-Audio-DynamicsCompressor, weiches Knie)
        const float ax = std::abs (x);
        compEnv = ax > compEnv ? compEnv + (ax - compEnv) * compA : compEnv * compR;
        const float db = 20.f * std::log10 (std::max (compEnv, 1e-6f));
        float out;
        const float d = db - kneeT;
        if (2.f * d < -knee) out = db;
        else if (2.f * std::abs (d) <= knee) out = db + (1.f / ratio - 1.f) * (d + knee * .5f) * (d + knee * .5f) / (2.f * knee);
        else out = kneeT + d / ratio;
        x *= std::pow (10.f, (out - db) / 20.f) * makeup;

        gMaster += (tMaster - gMaster) * smooth;
        x *= gMaster;
        const float a = std::abs (x);
        if (a > .9f) x = (x > 0 ? 1.f : -1.f) * (.9f + .1f * std::tanh ((a - .9f) / .1f));
        L[i] = x; if (R) R[i] = x;
        ++now;
    }
}

/* ---------- Patch-Logik ---------- */
bool Engine::gate (int src)
{
    if (src <= S_C8)
    {
        const int d = src == S_C1 ? 1 : src == S_C2 ? 2 : src == S_C4 ? 4 : 8;
        return stepNow % d == 0;
    }
    if (src <= S_SD)
    {
        const int r = src - S_SA; const int len = std::max (1, std::min (16, P.len[r]));
        return P.steps[r][stepNow % len] && ! P.mute[r] && (! anySolo || P.solo[r]);
    }
    if (isCv[src]) return prevCV[src] < .5f && cv[src] >= .5f;
    if (memo[src] >= 0) return memo[src] != 0;
    if (stack[src]) return false;
    stack[src] = true;
    bool r = false;
    if (src == S_PROB) r = inGate (I_PROB) && rand01() < P.prob;
    else if (src == S_DIV)
    {
        if (inGate (I_DIV)) { r = divCount % (1 + (int) std::lround (P.div * 7.f)) == 0; ++divCount; }
    }
    stack[src] = false; memo[src] = r ? 1 : 0;
    return r;
}

bool Engine::inGate (int inp)
{
    bool any = false;
    for (const auto& c : cables) if (c.to == inp && gate (c.from)) any = true;
    return any;
}

float Engine::inCV (int inp)
{
    float v = 0;
    for (const auto& c : cables)
        if (c.to == inp) v += isCv[c.from] ? cv[c.from] : (gate (c.from) ? 1.f : 0.f);
    return v;
}

void Engine::doStep (int64_t s)
{
    stepNow = s < 0 ? 0 : s;
    rndVal = rand01() * 2.f - 1.f;
    const double t = (double) now / sr;
    for (int i = 0; i < S_NUM; ++i) { isCv[i] = false; memo[i] = -1; stack[i] = false; }
    cv[S_L1] = lfo (1, t); cv[S_L2] = lfo (2, t); cv[S_RND] = rndVal;
    cv[S_EK] = envVal (0, now); cv[S_ES] = envVal (1, now); cv[S_EB] = envVal (2, now); cv[S_EH] = envVal (3, now);
    for (int i : { (int) S_L1, (int) S_L2, (int) S_RND, (int) S_EK, (int) S_ES, (int) S_EB, (int) S_EH }) isCv[i] = true;
    anySolo = P.solo[0] || P.solo[1] || P.solo[2] || P.solo[3];

    bool fires[kNumCh];
    for (int c = 0; c < kNumCh; ++c) fires[c] = inGate (inOf (c, 0));
    for (int c = 0; c < kNumCh; ++c)
        if (fires[c])
        {
            const float m = P.ch[c].mod * 2.f - 1.f;
            trigger (c, inCV (inOf (c, 1)) * m, inCV (inOf (c, 2)) * m, inCV (inOf (c, 3)) * m);
        }
    static const int litSrc[10] = { S_C1, S_C2, S_C4, S_C8, S_SA, S_SB, S_SC, S_SD, S_PROB, S_DIV };
    uint32_t lit = 0;
    for (int i = 0; i < 10; ++i) if (gate (litSrc[i])) lit |= 1u << i;
    litMask.fetch_or (lit);
    curStep.store (stepNow);
    for (int i = 0; i < S_NUM; ++i) prevCV[i] = cv[i];
}
} // namespace volta
