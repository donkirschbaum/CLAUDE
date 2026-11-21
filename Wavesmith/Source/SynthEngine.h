#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <atomic>
#include "Voice.h"

namespace ws
{

struct ClockSettings
{
    double bpm = 120.0;
    bool hostPlaying = false;
    double hostPpq = 0.0;

    int seqDiv = 3, seqLen = 16;
    float seqSmooth = 0.0f;
    float seqSteps[numSeqSteps] {};

    bool arpOn = false;
    int arpMode = 0, arpOct = 1, arpDiv = 3;
    float arpGate = 0.5f;

    bool lfoSync[2] { false, false };
    int lfoDiv[2] { 9, 9 };
};

// Voice management, MIDI handling, arpeggiator and step sequencer clock
class SynthEngine
{
public:
    static constexpr int maxVoices = 16;
    static constexpr int chunkSize = 16;

    void prepare (double sampleRate);
    void reset();

    // Renders into the (cleared) stereo buffer
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi,
                  const SynthSettings& settings, const ClockSettings& clock);

    std::atomic<int> currentSeqStep { 0 };

private:
    void handleMidi (const juce::MidiMessage& m, const SynthSettings& s, const ClockSettings& c);
    void keyDown (int note, float vel, const SynthSettings& s, const ClockSettings& c);
    void keyUp (int note, const SynthSettings& s, const ClockSettings& c);

    void voiceNoteOn (int note, float vel, const SynthSettings& s);
    void voiceNoteOff (int note, const SynthSettings& s);
    void allVoicesOff();

    void advanceClock (int numSamples, const SynthSettings& s, const ClockSettings& c);
    int nextArpNote (const ClockSettings& c);

    double sampleRate = 44100.0;
    std::array<Voice, maxVoices> voices;
    GlobalMod global;
    juce::Random random;

    // Physical keys held (in order of pressing) and their velocities
    std::vector<int> heldKeys;
    std::array<float, 128> keyVelocity {};
    bool sustainPedal = false;
    std::vector<int> sustainedKeys;

    // Mono mode note stack
    std::vector<int> monoStack;
    float lastNotePitch = -1.0f;

    // Clock
    double beatPos = 0.0;               // in quarter notes
    double lastArpStep = -1.0;
    int arpIndex = 0, arpDirection = 1;
    int arpCurrentNote = -1;
    double arpNoteOffBeat = -1.0;
    bool wasHostPlaying = false;

    float freeLfoPhase[2] { 0, 0 };
};

} // namespace ws
