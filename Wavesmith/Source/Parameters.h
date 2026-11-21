#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ws
{

inline const juce::StringArray& tableNames()
{
    static const juce::StringArray names {
        "Analog Morph", "Pulse Width", "Vowel Formants", "Harmonic Climb",
        "Hard Sync", "FM Bell", "Wavefold", "Bitcrush Steps",
        "Drawbar Organ", "Odd / Even", "Resonant Saw", "Spectral Noise",
        "Comb Saw", "Phase Distort", "Square Sync", "Glass Partials" };
    return names;
}

inline const juce::StringArray& syncDivNames()
{
    static const juce::StringArray names {
        "1/64", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
        "1/4T", "1/4", "1/4D", "1/2", "1/1", "2/1", "4/1" };
    return names;
}

// Length of each sync division in quarter notes (beats)
inline double syncDivBeats (int index)
{
    static const double beats[] { 0.0625, 0.125, 0.25 * 2.0 / 3.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75,
                                  2.0 / 3.0, 1.0, 1.5, 2.0, 4.0, 8.0, 16.0 };
    return beats[juce::jlimit (0, 14, index)];
}

inline const juce::StringArray& filterTypeNames()
{
    static const juce::StringArray names { "Low 12", "Low 24", "Band 12", "High 12", "Notch" };
    return names;
}

inline const juce::StringArray& lfoShapeNames()
{
    static const juce::StringArray names { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Sample & Hold", "Drift" };
    return names;
}

enum ModSource { srcNone = 0, srcModEnv, srcLfo1, srcLfo2, srcStepSeq, srcVelocity, srcModWheel, srcKeyTrack, srcRandom, srcAftertouch, numModSources };

inline const juce::StringArray& modSourceNames()
{
    static const juce::StringArray names { "-", "Mod Env", "LFO 1", "LFO 2", "Step Seq", "Velocity",
                                           "Mod Wheel", "Key Track", "Note Random", "Aftertouch" };
    return names;
}

enum ModDest { dstNone = 0, dstOscAPos, dstOscBPos, dstOscAPitch, dstOscBPitch, dstPitch, dstOscALevel, dstOscBLevel,
               dstOscADetune, dstOscBDetune, dstFm, dstSubLevel, dstNoiseLevel, dstCutoff, dstReso, dstDrive,
               dstAmp, dstPan, dstLfo1Rate, dstLfo2Rate, numModDests };

inline const juce::StringArray& modDestNames()
{
    static const juce::StringArray names { "-", "Osc A Position", "Osc B Position", "Osc A Pitch", "Osc B Pitch", "Pitch (All)",
                                           "Osc A Level", "Osc B Level", "Osc A Detune", "Osc B Detune", "FM Amount",
                                           "Sub Level", "Noise Level", "Filter Cutoff", "Filter Resonance", "Filter Drive",
                                           "Amp", "Pan", "LFO 1 Rate", "LFO 2 Rate" };
    return names;
}

inline const juce::StringArray& polyModeNames()
{
    static const juce::StringArray names { "Poly", "Mono", "Legato" };
    return names;
}

inline const juce::StringArray& arpModeNames()
{
    static const juce::StringArray names { "Up", "Down", "Up / Down", "Random", "As Played" };
    return names;
}

constexpr int numMatrixSlots = 6;
constexpr int numSeqSteps = 16;

inline juce::String seqStepId (int i)   { return "seqStep" + juce::String (i + 1); }
inline juce::String matrixId (int slot, const char* what) { return "m" + juce::String (slot + 1) + what; }

//==============================================================================
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&] (const String& id, const String& name, float min, float max, float def, float skewCentre = -1.0f, const String& suffix = {})
    {
        NormalisableRange<float> range (min, max);
        if (skewCentre > min && skewCentre < max)
            range.setSkewForCentre (skewCentre);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes().withLabel (suffix)));
    };
    auto addInt = [&] (const String& id, const String& name, int min, int max, int def)
    {
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, min, max, def));
    };
    auto addBool = [&] (const String& id, const String& name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto addChoice = [&] (const String& id, const String& name, const StringArray& choices, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, def));
    };

    // Oscillators
    for (auto osc : { "A", "B" })
    {
        const String p = String ("osc") + osc;
        const String n = String ("Osc ") + osc + " ";
        const bool isA = String (osc) == "A";
        addBool   (p + "On",     n + "On", isA);
        addChoice (p + "Table",  n + "Table", tableNames(), 0);
        addFloat  (p + "Pos",    n + "Position", 0.0f, 1.0f, isA ? 0.66f : 0.0f);
        addInt    (p + "Oct",    n + "Octave", -3, 3, 0);
        addInt    (p + "Semi",   n + "Semitone", -12, 12, 0);
        addFloat  (p + "Fine",   n + "Fine", -100.0f, 100.0f, 0.0f, -1.0f, "ct");
        addInt    (p + "Uni",    n + "Unison", 1, 8, 1);
        addFloat  (p + "Detune", n + "Detune", 0.0f, 1.0f, 0.2f);
        addFloat  (p + "Spread", n + "Stereo Spread", 0.0f, 1.0f, 0.7f);
        addFloat  (p + "Level",  n + "Level", 0.0f, 1.0f, 0.75f);
        addFloat  (p + "Pan",    n + "Pan", -1.0f, 1.0f, 0.0f);
        addFloat  (p + "Phase",  n + "Phase", 0.0f, 1.0f, 0.0f);
        addBool   (p + "Rand",   n + "Random Phase", true);
    }
    addFloat ("fmAmt", "FM B > A", 0.0f, 1.0f, 0.0f);

    // Sub & noise
    addFloat  ("subLevel", "Sub Level", 0.0f, 1.0f, 0.0f);
    addChoice ("subOct", "Sub Octave", StringArray { "-1 Oct", "-2 Oct" }, 0);
    addChoice ("subShape", "Sub Shape", StringArray { "Sine", "Triangle", "Square" }, 0);
    addFloat  ("noiseLevel", "Noise Level", 0.0f, 1.0f, 0.0f);
    addFloat  ("noiseColor", "Noise Color", 0.0f, 1.0f, 0.5f);

    // Filter
    addBool   ("fltOn", "Filter On", true);
    addChoice ("fltType", "Filter Type", filterTypeNames(), 1);
    addFloat  ("fltCutoff", "Cutoff", 20.0f, 20000.0f, 12000.0f, 1000.0f, "Hz");
    addFloat  ("fltReso", "Resonance", 0.0f, 1.0f, 0.1f);
    addFloat  ("fltDrive", "Filter Drive", 0.0f, 1.0f, 0.0f);
    addFloat  ("fltEnvAmt", "Filter Env Amount", -1.0f, 1.0f, 0.0f);
    addFloat  ("fltKey", "Filter Keytrack", 0.0f, 1.0f, 0.0f);
    addFloat  ("fltVel", "Filter Velocity", 0.0f, 1.0f, 0.0f);

    // Envelopes (amp + mod)
    for (auto e : { "amp", "env" })
    {
        const String p (e);
        const String n = p == "amp" ? "Amp " : "Mod Env ";
        const bool isAmp = p == "amp";
        addFloat (p + "A", n + "Attack",  0.0f, 10.0f, 0.002f, 0.5f, "s");
        addFloat (p + "D", n + "Decay",   0.001f, 10.0f, isAmp ? 0.3f : 0.4f, 0.8f, "s");
        addFloat (p + "S", n + "Sustain", 0.0f, 1.0f, isAmp ? 0.8f : 0.0f);
        addFloat (p + "R", n + "Release", 0.001f, 10.0f, isAmp ? 0.25f : 0.3f, 0.8f, "s");
    }
    addFloat ("ampVel", "Velocity Sensitivity", 0.0f, 1.0f, 0.4f);

    // LFOs
    for (auto l : { "lfo1", "lfo2" })
    {
        const String p (l);
        const String n = p == "lfo1" ? "LFO 1 " : "LFO 2 ";
        addChoice (p + "Shape",  n + "Shape", lfoShapeNames(), 0);
        addFloat  (p + "Rate",   n + "Rate", 0.01f, 30.0f, 2.0f, 2.0f, "Hz");
        addBool   (p + "Sync",   n + "Tempo Sync", false);
        addChoice (p + "Div",    n + "Sync Rate", syncDivNames(), 9);
        addBool   (p + "Retrig", n + "Retrigger", true);
    }

    // Step sequencer
    addChoice ("seqDiv", "Seq Rate", syncDivNames(), 3);
    addInt    ("seqLen", "Seq Length", 1, numSeqSteps, numSeqSteps);
    addFloat  ("seqSmooth", "Seq Smooth", 0.0f, 1.0f, 0.0f);
    for (int i = 0; i < numSeqSteps; ++i)
        addFloat (seqStepId (i), "Seq Step " + String (i + 1), 0.0f, 1.0f, (i % 4 == 0) ? 1.0f : 0.5f);

    // Arpeggiator
    addBool   ("arpOn", "Arp On", false);
    addChoice ("arpMode", "Arp Mode", arpModeNames(), 0);
    addInt    ("arpOct", "Arp Octaves", 1, 4, 1);
    addChoice ("arpDiv", "Arp Rate", syncDivNames(), 3);
    addFloat  ("arpGate", "Arp Gate", 0.05f, 1.0f, 0.5f);

    // Mod matrix
    for (int s = 0; s < numMatrixSlots; ++s)
    {
        addChoice (matrixId (s, "Src"), "Mod " + String (s + 1) + " Source", modSourceNames(), 0);
        addChoice (matrixId (s, "Dst"), "Mod " + String (s + 1) + " Destination", modDestNames(), 0);
        addFloat  (matrixId (s, "Amt"), "Mod " + String (s + 1) + " Amount", -1.0f, 1.0f, 0.0f);
    }

    // Voicing
    addChoice ("polyMode", "Voice Mode", polyModeNames(), 0);
    addInt    ("voices", "Voices", 1, 16, 12);
    addFloat  ("glide", "Glide", 0.0f, 2.0f, 0.0f, 0.3f, "s");
    addInt    ("bendRange", "Pitch Bend Range", 0, 24, 2);

    // Effects
    addBool  ("distOn", "Distortion On", false);
    addFloat ("distDrive", "Distortion Drive", 0.0f, 1.0f, 0.3f);
    addFloat ("distMix", "Distortion Mix", 0.0f, 1.0f, 1.0f);
    addBool  ("chorusOn", "Chorus On", false);
    addFloat ("chorusRate", "Chorus Rate", 0.05f, 5.0f, 0.6f, 1.0f, "Hz");
    addFloat ("chorusDepth", "Chorus Depth", 0.0f, 1.0f, 0.4f);
    addFloat ("chorusMix", "Chorus Mix", 0.0f, 1.0f, 0.4f);
    addBool   ("delayOn", "Delay On", false);
    addChoice ("delayDiv", "Delay Time", syncDivNames(), 7);
    addFloat  ("delayFb", "Delay Feedback", 0.0f, 0.95f, 0.35f);
    addFloat  ("delayMix", "Delay Mix", 0.0f, 1.0f, 0.25f);
    addFloat  ("delayTone", "Delay Tone", 0.0f, 1.0f, 0.6f);
    addBool  ("reverbOn", "Reverb On", false);
    addFloat ("reverbSize", "Reverb Size", 0.0f, 1.0f, 0.6f);
    addFloat ("reverbDamp", "Reverb Damping", 0.0f, 1.0f, 0.5f);
    addFloat ("reverbMix", "Reverb Mix", 0.0f, 1.0f, 0.2f);
    addFloat ("reverbWidth", "Reverb Width", 0.0f, 1.0f, 1.0f);

    addFloat ("masterVol", "Master Volume", -48.0f, 6.0f, -6.0f, -1.0f, "dB");

    return layout;
}

} // namespace ws
