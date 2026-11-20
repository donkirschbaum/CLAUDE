#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Dsp.h"

namespace fs
{
constexpr int kLanes = 5, kSteps = 16, kMel = 4;
constexpr int kMaxVoices = 64, kMaxEvents = 512;

struct Style
{
    const char* name; float bpm, swing, ratchet;
    float kick[16], snare[16], hat[16], perc[16];
};
extern const Style kStyles[6];

struct Scale { const char* name; int n; int deg[7]; };
extern const Scale kScales[5];

struct Event
{
    juce::int64 when = 0;
    int lane = 0;     // 0..4 Spur, -1 Playhead-Marker, -2 Vorhoeren ohne Spur
    float vel = 1;
    int val = 1;      // Drums: 1, Melodie: MIDI-Note, Marker: Schritt
    int slice = -1;   // -1 = Schnipsel der Spur
    bool seq = true;  // aus dem Sequencer (Melodie mit Oktav-Ausgleich)
};

struct Voice
{
    bool active = false, isSub = false, releasing = false;
    const float* data = nullptr; int length = 0;
    double pos = 0, rate = 1, phase = 0;
    float gain = 0, env = 1, coef = 1, relCoef = 1;
    int hold = 0, age = 0, lane = 0;
    int ftype = 0; float a1 = 0, a2 = 0, a3 = 0, k = 1.414f, ic1 = 0, ic2 = 0;
};
} // namespace fs

class FundstueckProcessor : public juce::AudioProcessor
{
public:
    FundstueckProcessor();
    ~FundstueckProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Fundstueck"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ---- Message-Thread-API (fuer die Oberflaeche) ----
    enum class Mode { Fresh, Resens, Restore };
    void loadDemo();
    bool loadFiles (const juce::StringArray& paths);
    void reanalyze (Mode mode);
    void autoAssign (bool random);
    void rollDrums();
    void rollMel();
    void clearPattern();
    void setStyle (int s);
    void cycleSlice (int lane, int dir);
    void setLaneSlice (int lane, int idx);
    void audition (int slice, int lane);
    void stepClick (int lane, int step, int mod);
    void shiftNote (int lane, int step, int dir);
    std::vector<int> scaleNotes() const;
    void updateMelOct();
    void bump() { ++uiVersion; }

    std::shared_ptr<fs::SliceSet> msgSet;
    std::vector<float> source;
    double sourceSR = 44100.0;
    juce::String sourceLabel, statusText;
    float sens = 0.55f, density = 0.5f;
    int style = 0, root = 9, scale = 0;

    std::atomic<int> steps[fs::kLanes][fs::kSteps];
    std::atomic<int> laneSlice[fs::kLanes];
    std::atomic<int> melOct { 0 };
    std::atomic<bool> internalPlay { false }, hostPlaying { false };
    std::atomic<int> currentStep { -1 };
    std::atomic<int> uiVersion { 0 };
    std::atomic<double> curBpm { 90.0 };
    std::atomic<double> hostBpm { 0.0 };     // 0 = Host liefert kein Tempo
    std::atomic<bool> syncOn { false };

    // Aufnahme vom Eingang (Standalone: Mikrofon, Ableton: Sidechain)
    void startRecording();
    void stopRecording();
    std::atomic<bool> recording { false };
    std::atomic<bool> recFull { false };
    std::atomic<int> recPos { 0 };
    std::atomic<float> inLevel { 0.0f };
    double getRecSampleRate() const { return sr; }

private:
    void pushEvent (const fs::Event& e);
    void scheduleStep (int step, juce::int64 when, double stepS);
    void fire (const fs::Event& e);
    void startVoice (const fs::Slice& s, int lane, double rate, float gain, float len, int ftype, float freq);
    void startSub (float gain);
    void render (int from, int to);

    // Audio-Thread-Zustand
    double sr = 44100.0;
    juce::int64 clock = 0;
    double internalPpq = 0, expectedPpq = -1.0;
    juce::int64 nextK = 0;
    bool wasRunning = false, wasInternal = false;
    std::shared_ptr<fs::SliceSet> audioSet, nextSet;
    juce::SpinLock setLock;
    std::atomic<bool> setPending { false };
    fs::Voice voices[fs::kMaxVoices];
    fs::Event events[fs::kMaxEvents];
    int numEvents = 0;
    std::vector<float> mix;
    juce::Reverb reverb;
    juce::Random audioRand, msgRand;

    juce::AbstractFifo uiFifo { 64 };
    fs::Event uiEvents[64];

    std::atomic<float>* pBpm = nullptr; std::atomic<float>* pSwing = nullptr; std::atomic<float>* pFeel = nullptr;
    std::atomic<float>* pRev = nullptr; std::atomic<float>* pTape = nullptr; std::atomic<float>* pSub = nullptr;
    std::atomic<float>* pMaster = nullptr; std::atomic<float>* pSync = nullptr;
    std::vector<float> recBuf;
    std::atomic<float>* pVol[fs::kLanes] {}; std::atomic<float>* pTune[fs::kLanes] {};
    std::atomic<float>* pLen[fs::kLanes] {}; std::atomic<float>* pMute[fs::kLanes] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FundstueckProcessor)
};
