#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "SampleLibrary.h"
#include <array>
#include <atomic>
#include <numeric>

namespace shaker
{
constexpr int kTracks = 4;
constexpr int kMaxSteps = 32;
constexpr int kVoicesPerTrack = 12;

// Tempo pro Spur: Zähler/Nenner
struct Rate { const char* label; int num; int den; };
constexpr Rate kRates[] = { { "/4", 1, 4 }, { "/3", 1, 3 }, { "/2", 1, 2 }, { "/1.5", 2, 3 }, { "x1", 1, 1 },
                            { "x1.5", 3, 2 }, { "x2", 2, 1 }, { "x3", 3, 1 }, { "x4", 4, 1 } };
constexpr int kNumRates = 9;
constexpr int kRateX1 = 4;

enum Mode { Polymeter = 0, Polyrhythm = 1 };

struct TrackParams
{
    float gain = 0, tune = 0, fine = 0, attack = 0, decay = 180, sustain = 0, release = 150;
    float lowcut = 20, highcut = 20000, reso = 0.707f, bodyF = 420, body = 0, air = 0, drive = 0, comp = 0, width = 1;
    float vol = 0.8f, pan = 0, rev = 0.15f;
    bool mute = false, solo = false;
    int steps = 16, mode = Polymeter, rate = kRateX1;
    float rProb = 0, rTime = 0.15f, rVel = 0.2f, rPitch = 0, rLen = 0, rStart = 0, rRev = 0, rPan = 0.1f, rRatchet = 0;
};

struct GlobalParams
{
    float swing = 0.1f, chaos = 0, evolve = 0, drift = 0, density = 0, ghost = 0, room = 0, roomSize = 0, master = 0.8f, tempo = 96;
};

struct Transport { bool playing = false; double ppq = 0; double bpm = 120; };

// Zwischen UI und Audio-Thread geteilter Zustand einer Spur
struct TrackShared
{
    std::array<std::atomic<float>, kMaxSteps> pat {};
    std::atomic<int> sample { -1 };
    std::atomic<float> start { 0.0f }, end { 1.0f };   // Sekunden im Sample
    std::atomic<int> curStep { -1 };
    std::atomic<unsigned int> hits { 0 };
    std::atomic<int> audition { 0 };                     // 1 = anspielen, 2 = Ausschnitt sauber vorhören
    std::atomic<bool> patternChanged { false };
    std::atomic<double> previewStartMs { 0.0 };
    std::atomic<float> previewRate { 1.0f };
};

// Cycle-Länge in Vierteln als Bruch
inline void cycleFraction (int steps, int mode, int rate, long& num, long& den)
{
    const auto& r = kRates[juce::jlimit (0, kNumRates - 1, rate)];
    if (mode == Polyrhythm) { num = 4L * r.den; den = r.num; }
    else                    { num = (long) steps * r.den; den = 4L * r.num; }
    auto g = std::gcd (num, den);
    num /= g; den /= g;
}
inline double cycleBeats (int steps, int mode, int rate)
{
    long n, d; cycleFraction (steps, mode, rate, n, d);
    return (double) n / (double) d;
}

class Biquad
{
public:
    void setLowpass (double sr, double f, double q)   { calc (0, sr, f, q, 0); }
    void setHighpass (double sr, double f, double q)  { calc (1, sr, f, q, 0); }
    void setPeak (double sr, double f, double q, double db)  { calc (2, sr, f, q, db); }
    void setHighShelf (double sr, double f, double db)        { calc (3, sr, f, 0.707, db); }
    inline float process (float x, int ch) noexcept
    {
        auto& s = st[ch];
        const double y = b0 * x + s.z1;
        s.z1 = b1 * x - a1 * y + s.z2;
        s.z2 = b2 * x - a2 * y;
        return (float) y;
    }
    void reset() { st[0] = st[1] = {}; }
    bool bypass = false;
private:
    void calc (int type, double sr, double f, double q, double db);
    struct State { double z1 = 0, z2 = 0; };
    State st[2];
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

struct Voice
{
    bool active = false;
    const SampleData* s = nullptr;
    double pos = 0, inc = 1;
    int regionStart = 0, regionEnd = 0;
    bool reverse = false;
    int delay = 0;          // Start-Versatz innerhalb des Blocks
    int stage = 0;          // 0 Attack, 1 Decay/Sustain, 2 Release
    float level = 0, peak = 0, sus = 0, atkInc = 0, decCoef = 0, relCoef = 0;
    int gateLeft = 0;
    float gl = 1, gr = 1;
    int fadeLen = 1;
    juce::uint32 age = 0;
};

class Engine
{
public:
    explicit Engine (SampleLibrary& lib);
    void prepare (double sampleRate, int maxBlock);
    void process (juce::AudioBuffer<float>& out, const Transport& tr, const GlobalParams& g, const TrackParams* tp);

    TrackShared shared[kTracks];

private:
    void scheduleTrack (int ti, const TrackParams& p, const GlobalParams& g, double b0, double b1, double spb, bool jumped, int numSamples);
    void playStep (int ti, int idx, int offset, double stepSec, const TrackParams& p, const GlobalParams& g, long cycle);
    void startVoice (int ti, int offset, float vel, double gateSec, const TrackParams& p, float chaos, bool clean);
    void renderVoice (Voice& v, float* L, float* R, int n);
    void updateStrip (int ti, const TrackParams& p);
    float gauss();

    SampleLibrary& lib;
    double sr = 44100.0;
    juce::Random rng;
    juce::uint32 voiceAge = 0;

    struct TrackRuntime
    {
        Voice voices[kVoicesPerTrack];
        long lastGs = -1;
        double lastSb = -1;
        long lastCycle = -1;
        float drift = 0;
        Biquad hp, lp, body, air;
        TrackParams last;
        bool stripInit = false;
        float compEnv = 0;
        juce::AudioBuffer<float> buf;
    } tr[kTracks];

    juce::AudioBuffer<float> revBuf;
    juce::Reverb reverb;
    Biquad revHp;
    float lastRoomSize = -1;
    double prevEnd = -1;
    bool wasPlaying = false;
    bool anySolo = false;
    float limEnv = 1.0f;
};
} // namespace shaker
