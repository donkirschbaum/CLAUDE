#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <optional>
#include <deque>
#include "Engine.h"

namespace g8
{
// ---------- Aufnahme-Strukturen ----------
struct CapEvent { uint8_t s, t, v; float o; };
struct Bar
{
    int nEvents = 0;
    CapEvent ev[NT * 16];
    uint8_t params[16][NPARAMS_SNAP];
    int nSteps = 0;
    float bpm = 120.f, swing = 50.f;
    float snd[NT][4]; // vol, pitch, decay, pan
};
struct SlotEvent { int s; uint8_t t, v; float o; };
struct SlotData
{
    int bars = 0;
    float bpm = 120.f, swing = 50.f;
    std::vector<SlotEvent> ev;
    std::vector<std::array<uint8_t, NPARAMS_SNAP>> params;
};
struct SlotCfg { int len = 4; bool lock = false; };
struct PlaySlot { int bars = 1; std::vector<std::vector<SlotEvent>> byStep; };

// ---------- Sampler ----------
struct SampleData { juce::AudioBuffer<float> buf; double sr = 44100.0; juce::String name; };
struct Voice
{
    bool active = false;
    const SampleData* s = nullptr;
    double pos = 0, inc = 1;
    float gL = 1, gR = 1, env = 1, envMul = 1, fade = 1, fadeStep = 0;
    bool fading = false;
    int age = 0, hold = 0, dur = 0, delay = 0, track = 0;
    bool useDecay = false;
};

class Grids8Processor : public juce::AudioProcessor, private juce::Timer
{
public:
    Grids8Processor();
    ~Grids8Processor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Grids 8"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---------- öffentlich für den Editor ----------
    juce::AudioProcessorValueTreeState apvts;
    static juce::String tid (int t, const char* key) { return "t" + juce::String (t + 1) + key; }
    float pv (const juce::String& id) const;
    void setParam (const juce::String& id, float value);

    const PatternMap& map() const { return maps[(size_t) activeMap.load()]; }
    uint32_t getSeed() const { return seed.load(); }
    void newMap (uint32_t s);
    EffParams effParamsUI (int t, int64_t g) const;
    void masterEffUI (int64_t g, float& x, float& y) const;

    std::atomic<int64_t> uiStep { -1 };
    std::atomic<uint32_t> hitCount[NT] {};
    std::atomic<bool> fillHeld { false }, internalPlay { false };
    std::atomic<float> hostBpm { 120.f };
    std::atomic<bool> hostPlaying { false };
    std::atomic<int64_t> masterMoveStep { INT64_MIN };
    juce::String getTrackName (int t) const { return trackNames[t]; }
    void setTrackName (int t, const juce::String& n) { trackNames.set (t, n.trim().isEmpty() ? juce::String (TRACKS[t].name) : n.trim().substring (0, 16)); ++slotsVersion; }
    bool seqCollapsed = false;

    // Zufall
    void randomize (const juce::String& kind);
    void diceTrack (int t, bool onlyXY);

    // Slots (Message-Thread)
    juce::CriticalSection slotLock;
    std::array<std::optional<SlotData>, 8> slots;
    std::array<SlotCfg, 8> cfg;
    std::deque<Bar> history;
    std::atomic<int> slotsVersion { 0 };
    int loopSlot = -1;
    void setSlotLen (int i, int len);
    void toggleHold (int i);
    void clearSlot (int i);
    void toggleLoop (int i);
    void holdAll();
    juce::File writeSlotMidi (int i, bool multiTrack, juce::File target = {});
    juce::String slotMeta (int i);

    // Samples
    juce::String sampleName (int t) const { return samples[(size_t) t] ? samples[(size_t) t]->name : juce::String(); }
    bool loadUserSample (int t, const juce::File& f);

    static constexpr int HIST_MAX = 16;
    void pumpMessageThreadWork() { timerCallback(); } // fuer Tests

private:
    void timerCallback() override;
    void loadDefaultSamples();
    void doStep (int64_t k, double stepTime, double stepSamples);
    void startVoice (int t, int vel, double atSample);
    void renderVoices (float* L, float* R, int n);
    EffParams effParams (int t, int64_t g, float mx, float my, float md, bool audio) const;
    void masterEff (int64_t g, float& x, float& y, float& d) const;
    float fillBoost (int t, int64_t g) const;
    float trackMod (int t, int64_t g, int axis) const;
    void pushBar();
    void updateLiveSlots();
    void fillSlot (int i);
    void rebuildPlaySlot();
    int noteFor (int t) const;
    void serializeSlots (juce::ValueTree& vt);
    void deserializeSlots (const juce::ValueTree& vt);

    std::array<PatternMap, 2> maps;
    std::atomic<int> activeMap { 0 };
    std::atomic<uint32_t> seed { 20251120u };

    float perturb[NT][NSTEP] {};
    juce::Random rng, uiRng;

    // Parameter-Zeiger
    struct TP { std::atomic<float> *x, *y, *dens, *chaos, *vol, *pitch, *decay, *pan, *len, *mute, *solo, *mShape, *mRate, *mDx, *mDy, *mDd,
                *mAmt, *mRot, *mPhase, *mAtk, *mDec; };
    std::array<TP, NT> tp {};
    std::atomic<float> *mX, *mY, *mDens, *mChaos, *mVol, *mShape, *mRate, *mDx, *mDy, *mDd, *swing, *intBpm,
        *hTime, *hVel, *afOn, *afEvery, *choke, *sound, *noteMap, *midiCh, *evOn, *evEvery, *rndAmt;

    // Transport
    double sr = 44100.0;
    int64_t sampleCounter = 0, lastStep = INT64_MIN;
    bool wasPlaying = false;
    double internalPpq = 0;

    struct Pending { double time; int track; int vel; bool isOff; int note; int ch; };
    std::vector<Pending> pending;

    std::array<Voice, 48> voices;
    std::array<std::unique_ptr<SampleData>, NT> samples;
    std::vector<std::unique_ptr<SampleData>> graveyard;
    juce::SpinLock sampleLock;

    // Aufnahme
    Bar curBar;
    bool curValid = false;
    juce::AbstractFifo barFifo { 32 };
    std::array<Bar, 32> barBuf;
    std::atomic<int64_t> barCounter { 0 };
    int64_t lastBarSeen = 0;

    // Loop-Wiedergabe
    std::shared_ptr<const PlaySlot> playSlot;
    juce::SpinLock playLock;

    // Glide
    struct Glide { bool on = false; int64_t g0 = 0; float x0, y0, x1, y1; } glide;

    juce::StringArray userSamplePaths;
    juce::StringArray trackNames;
    float lastMX = -1, lastMY = -1; bool masterMoving = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Grids8Processor)
};
} // namespace g8
