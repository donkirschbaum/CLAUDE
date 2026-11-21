// VOLTA·4 – semimodularer Drum-Synth. Klangerzeugung (frei von JUCE).
#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace volta
{
constexpr int kNumCh = 4;      // 0 Kick, 1 Snare, 2 Bass, 3 Hats  (= SEQ A, B, C, D)
constexpr int kVoices = 4;     // überlappende Ausklänge pro Kanal

enum Src { S_C1, S_C2, S_C4, S_C8, S_SA, S_SB, S_SC, S_SD, S_L1, S_L2, S_RND,
           S_EK, S_ES, S_EB, S_EH, S_PROB, S_DIV, S_NUM };
// Eingänge: 0 PROB IN, 1 TEILER IN, danach je Kanal TRIG, PITCH, DECAY, FARBE
enum In { I_PROB = 0, I_DIV = 1, I_NUM = 2 + kNumCh * 4 };
inline int inOf (int ch, int k) { return 2 + ch * 4 + k; }   // k: 0 trig, 1 pitch, 2 dec, 3 tone

int srcFromId (const std::string& id);   // "sA", "l1", "eK" ... ; -1 wenn unbekannt
int inFromId (const std::string& id);    // "kick.trig", "probI" ... ; -1 wenn unbekannt

struct Cable { int from, to; };

struct ChParams { float tune = .5f, decay = .3f, tone = .5f, snap = .5f, mod = .75f, level = .7f, fx = 0.f; };

struct Params
{
    float bpm = .45f, swing = 0, vol = .75f, dtime = .5f, dfb = .35f, dmix = .6f;
    float l1r = .3f, l2r = .15f, prob = .5f, div = .29f;
    int l1s = 0, l2s = 1;
    ChParams ch[kNumCh];
    bool steps[4][16] {};
    int len[4] { 16, 16, 16, 16 };
    bool mute[4] {}, solo[4] {};
};

/* ---------- DSP-Bausteine ---------- */
struct Biquad
{
    enum Type { LP, HP, BP };
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (Type t, float f, float q, float sr);
    inline float p (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
};

// Exponentielle Hüllkurve wie Web Audio: 2 ms Anstieg auf peak, dann exponentiell auf 0.0001
struct Env
{
    float v = 0, aMul = 1, dMul = 1; int aN = 1, n = 0, total = 0; bool active = false;
    void start (float peak, float dec, float sr);
    inline float next() { if (! active) return 0.f; const float o = v; v *= (n < aN ? aMul : dMul); if (++n >= total) active = false; return o; }
};

struct Ramp
{
    float v = 1, mul = 1; int n = 0;
    void start (float from, float to, float time, float sr);
    inline float next() { const float o = v; if (n > 0) { v *= mul; --n; } return o; }
};

struct Noise
{
    uint32_t s = 0x9E3779B9u;
    inline float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.0f / 2147483648.0f); }
};

/* ---------- Stimmen ---------- */
struct KickVoice  { bool on = false; Env amp, click; Ramp fr; float ph = 0; Biquad lp, hp; bool hasClick = false; float gain = 1; };
struct SnareVoice { bool on = false; Env body, nz; Ramp fr; float ph = 0; Biquad bp; float gain = 1; };
struct BassVoice  { bool on = false; Env amp; Ramp cut; float ph1 = 0, ph2 = 0, f = 110, q = 1; int cnt = 0; Biquad lp; float gain = 1; };
struct HatVoice   { bool on = false; Env amp; float ph[6] {}, fr[6] {}, mg = .3f, ng = .5f; Biquad bp, hp; float gain = 1; };

class Engine
{
public:
    void prepare (double sampleRate);
    void setParams (const Params& p) { P = p; }
    void setCables (const std::vector<Cable>& c) { cables = c; }

    // Audio bis zum nächsten Ereignis rendern (mono -> L/R)
    void render (float* L, float* R, int n);
    // Sequenzerschritt s am aktuellen Zeitpunkt auswerten
    void doStep (int64_t s);
    // Kanal anschlagen. pitch/dec/tone: bereits mit CV-Tiefe skaliert. midiNote >= 0: Bass-Tonhöhe fest
    void trigger (int ch, float pitch, float dec, float tone, float gain = 1.f, int midiNote = -1);
    void resetSequencer();
    float bpmFromParam() const { return std::round (60.f + P.bpm * 140.f); }

    // Anzeige (vom Editor gelesen)
    std::atomic<uint32_t> litMask { 0 };   // Bits 0..9: c1 c2 c4 c8 sA sB sC sD prob div; 16..19: Kanäle
    std::atomic<int64_t> curStep { -1 };
    std::array<std::atomic<float>, 7> cvShow {};   // l1 l2 rnd eK eS eB eH

    void publishCV();
    void setStepDuration (double sec) { stepSec = sec; }

private:
    float renderKick (KickVoice&); float renderSnare (SnareVoice&); float renderBass (BassVoice&); float renderHat (HatVoice&);
    float lfo (int i, double t) const;
    float envVal (int ch, int64_t at) const;
    bool gate (int src);
    bool inGate (int inp);
    float inCV (int inp);

    double sr = 44100; Params P; std::vector<Cable> cables;
    int64_t now = 0;               // Samplezähler
    double stepSec = .125;
    KickVoice kick[kVoices]; SnareVoice snare[kVoices]; BassVoice bass[kVoices]; HatVoice hats[kVoices];
    int rr[kNumCh] {};
    int64_t lastTrig[kNumCh] { -100000000, -100000000, -100000000, -100000000 };
    float lastDec[kNumCh] { .3f, .3f, .3f, .3f };
    Noise noise; uint32_t rng = 12345;
    float rand01() { rng = rng * 1664525u + 1013904223u; return (rng >> 8) * (1.0f / 16777216.0f); }

    // Schritt-Auswertung
    float cv[S_NUM] {}; bool isCv[S_NUM] {}; float prevCV[S_NUM] {};
    int8_t memo[S_NUM] {}; bool stack[S_NUM] {};
    int64_t stepNow = 0; bool anySolo = false; int divCount = 0; float rndVal = 0;

    // Mischpult
    float gLevel[kNumCh] {}, gFx[kNumCh] {}, gMaster = 0, smooth = 0.001f;
    std::vector<float> dl; int dw = 0; float dTime = .25f; Biquad dlp;
    float compEnv = 0, compA = 0, compR = 0;
};
} // namespace volta
