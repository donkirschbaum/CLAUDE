#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <numeric>
#include <mutex>
#include <set>
#include <algorithm>

namespace
{
    // Octave convention: C3 = MIDI 60 (Ableton, Logic, Cubase) -> C1 = 36
    constexpr int middleCOctave = 3;

    juce::String noteName (int note)
    {
        return juce::MidiMessage::getMidiNoteName (note, true, true, middleCOctave);
    }

    int parseNoteName (const juce::String& textIn)
    {
        auto text = textIn.trim().toUpperCase();
        if (text.isEmpty())
            return 36;

        if (text.containsOnly ("0123456789"))
            return juce::jlimit (0, 127, text.getIntValue());

        static const int semis[] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
        const auto c = text[0];
        if (c < 'A' || c > 'G')
            return juce::jlimit (0, 127, text.getIntValue());

        int semitone = semis[c - 'A'];
        int pos = 1;
        if (text[pos] == '#') { ++semitone; ++pos; }
        else if (text[pos] == 'B' && text.length() > 2) { --semitone; ++pos; }

        const int octave = text.substring (pos).getIntValue();
        return juce::jlimit (0, 127, (octave + (5 - middleCOctave)) * 12 + semitone);
    }

    std::mutex instanceMutex;
    std::set<int> usedInstances;

    int acquireInstanceNumber()
    {
        std::lock_guard<std::mutex> lg (instanceMutex);
        int n = 1;
        while (usedInstances.count (n) != 0)
            ++n;
        usedInstances.insert (n);
        return n;
    }

    void releaseInstanceNumber (int n)
    {
        std::lock_guard<std::mutex> lg (instanceMutex);
        usedInstances.erase (n);
    }

    // C1, C#1, F#1, G#1, B1, C2
    const int defaultNotes[PolyRhythmProcessor::numTracks] = { 36, 37, 42, 44, 47, 48 };

    const int defaultLengths[PolyRhythmProcessor::numTracks] = { 32, 24, 20, 16, 12, 10 };
}

//==============================================================================
juce::StringArray PolyRhythmProcessor::getRateNames()
{
    return { "1/32", "1/16", "1/8", "1/4", "1/16T", "1/8T" };
}

double PolyRhythmProcessor::rateIndexToQuarters (int index)
{
    switch (index)
    {
        case 0:  return 0.125;
        case 1:  return 0.25;
        case 2:  return 0.5;
        case 3:  return 1.0;
        case 4:  return 1.0 / 6.0;
        case 5:  return 1.0 / 3.0;
        default: return 0.125;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout PolyRhythmProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int t = 0; t < numTracks; ++t)
    {
        const auto prefix = "Spur " + juce::String (t + 1) + " ";

        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { id ("len", t), 1 }, prefix + "Laenge", 1, maxSteps, defaultLengths[t],
            juce::AudioParameterIntAttributes()
                .withStringFromValueFunction ([] (int v, int) { return juce::String (v) + "/32"; })
                .withValueFromStringFunction ([] (const juce::String& s) { return s.getIntValue(); })));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id ("rate", t), 1 }, prefix + "Rate", getRateNames(), 0));

        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { id ("note", t), 1 }, prefix + "Note", 0, 127, defaultNotes[t],
            juce::AudioParameterIntAttributes()
                .withStringFromValueFunction ([] (int v, int) { return noteName (v); })
                .withValueFromStringFunction ([] (const juce::String& s) { return parseNoteName (s); })));

        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { id ("vel", t), 1 }, prefix + "Velocity", 1, 127, 100));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id ("gate", t), 1 }, prefix + "Gate",
            juce::NormalisableRange<float> (0.05f, 1.0f, 0.01f), 0.5f,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { id ("mute", t), 1 }, prefix + "Mute", false));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "density", 1 }, "Zufall Dichte",
        juce::NormalisableRange<float> (0.05f, 1.0f, 0.01f), 0.35f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "randNotes", 1 }, "Zufall inkl. Noten", false));

    auto noteAttr = juce::AudioParameterIntAttributes()
                        .withStringFromValueFunction ([] (int v, int) { return noteName (v); })
                        .withValueFromStringFunction ([] (const juce::String& s) { return parseNoteName (s); });

    // C0 .. C8 (C3 = 60)
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "noteLo", 1 }, "Zufall Noten von", 24, 120, 36, noteAttr));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "noteHi", 1 }, "Zufall Noten bis", 24, 120, 48, noteAttr));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "bpm", 1 }, "Interne BPM",
        juce::NormalisableRange<float> (30.0f, 300.0f, 0.1f), 120.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "play", 1 }, "Interner Play", false));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "channel", 1 }, "MIDI Kanal", 1, 16, 1));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "virtOut", 1 }, "Virtueller MIDI-Ausgang", true));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "exportLen", 1 }, "Export Laenge",
        juce::StringArray { "Auto (Loop)", "1 Takt", "2 Takte", "4 Takte", "8 Takte", "16 Takte", "32 Takte" }, 0));

    return layout;
}

//==============================================================================
PolyRhythmProcessor::PolyRhythmProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PolyRhythm32", createLayout())
{
    // A musical starting point
    patterns[0].store (0x01010101u); // four on the floor (1/32 grid -> every quarter)
    patterns[1].store (0x10101010u); // backbeat
    patterns[2].store (0x44444444u); // offbeat 1/8
    juce::Random r (4711);
    for (int t = 3; t < numTracks; ++t)
    {
        uint32_t bits = 0;
        for (int s = 0; s < maxSteps; ++s)
            if (r.nextFloat() < 0.3f)
                bits |= (1u << s);
        patterns[(size_t) t].store (bits | 1u);
    }

    for (auto& s : currentStep)
        s.store (-1);

    instanceNumber = acquireInstanceNumber();
    startTimerHz (2);
}

PolyRhythmProcessor::~PolyRhythmProcessor()
{
    stopTimer();
    {
        const juce::SpinLock::ScopedLockType sl (virtualOutLock);
        virtualOut.reset();
    }
    releaseInstanceNumber (instanceNumber);
}

//==============================================================================
bool PolyRhythmProcessor::virtualPortSupported()
{
   #if JUCE_MAC || JUCE_LINUX || JUCE_IOS
    return true;
   #else
    return false;
   #endif
}

juce::String PolyRhythmProcessor::getVirtualPortName() const
{
    return virtualPortName;
}

void PolyRhythmProcessor::timerCallback()
{
    updateVirtualPort();
}

void PolyRhythmProcessor::updateVirtualPort()
{
    if (! virtualPortSupported())
        return;

    const bool want = apvts.getRawParameterValue ("virtOut")->load() > 0.5f;
    const bool have = virtualOut != nullptr;

    if (want && ! have)
    {
        const auto name = juce::String ("PolyRhythm32 Out") + (instanceNumber > 1 ? " " + juce::String (instanceNumber) : juce::String());
        if (auto out = juce::MidiOutput::createNewDevice (name))
        {
            out->startBackgroundThread();
            const juce::SpinLock::ScopedLockType sl (virtualOutLock);
            virtualOut = std::move (out);
            virtualPortName = name;
        }
    }
    else if (! want && have)
    {
        std::unique_ptr<juce::MidiOutput> old;
        {
            const juce::SpinLock::ScopedLockType sl (virtualOutLock);
            old = std::move (virtualOut);
            virtualPortName = {};
        }
        if (old != nullptr)
            old->stopBackgroundThread();
    }
}

bool PolyRhythmProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void PolyRhythmProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    for (auto& v : voices)
        v = Voice();
    wasPlaying = false;
}

//==============================================================================
bool PolyRhythmProcessor::getStep (int track, int step) const
{
    return ((patterns[(size_t) track].load() >> step) & 1u) != 0;
}

void PolyRhythmProcessor::setStep (int track, int step, bool on)
{
    auto& p = patterns[(size_t) track];
    auto bits = p.load();
    if (on) bits |= (1u << step);
    else    bits &= ~(1u << step);
    p.store (bits);
}

int PolyRhythmProcessor::getTrackLength (int track) const
{
    return juce::jlimit (1, maxSteps, (int) apvts.getRawParameterValue (id ("len", track))->load());
}

double PolyRhythmProcessor::getTrackStepQuarters (int track) const
{
    return rateIndexToQuarters ((int) apvts.getRawParameterValue (id ("rate", track))->load());
}

int PolyRhythmProcessor::getTrackNote (int track) const
{
    return (int) apvts.getRawParameterValue (id ("note", track))->load();
}

void PolyRhythmProcessor::setParamValue (const juce::String& paramId, float plainValue)
{
    if (auto* p = apvts.getParameter (paramId))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

int PolyRhythmProcessor::pickRandomRate()
{
    // weighted: 1/32, 1/16, 1/8, 1/4, 1/16T, 1/8T
    static const int weights[] = { 3, 3, 2, 1, 1, 1 };
    int total = 0;
    for (auto w : weights) total += w;
    int r = random.nextInt (total);
    for (int i = 0; i < 6; ++i)
    {
        if (r < weights[i]) return i;
        r -= weights[i];
    }
    return 0;
}

void PolyRhythmProcessor::randomizeTrackInternal (int track, std::vector<int>& usedNotes)
{
    const float density  = apvts.getRawParameterValue ("density")->load();
    const bool randNotes = apvts.getRawParameterValue ("randNotes")->load() > 0.5f;

    // length, rate, velocity, gate
    const int len = random.nextInt (juce::Range<int> (2, maxSteps + 1));
    setParamValue (id ("len", track), (float) len);
    setParamValue (id ("rate", track), (float) pickRandomRate());
    setParamValue (id ("vel", track), (float) random.nextInt (juce::Range<int> (60, 128)));
    setParamValue (id ("gate", track), 0.1f + 0.9f * random.nextFloat());

    // notes only if switched on: inside the selected range, different per track if possible
    if (randNotes)
    {
        int lo = (int) apvts.getRawParameterValue ("noteLo")->load();
        int hi = (int) apvts.getRawParameterValue ("noteHi")->load();
        if (lo > hi) std::swap (lo, hi);

        std::vector<int> candidates;
        for (int n = lo; n <= hi; ++n)
            if (std::find (usedNotes.begin(), usedNotes.end(), n) == usedNotes.end())
                candidates.push_back (n);

        const int note = candidates.empty() ? random.nextInt (juce::Range<int> (lo, hi + 1))
                                            : candidates[(size_t) random.nextInt ((int) candidates.size())];
        setParamValue (id ("note", track), (float) note);
        usedNotes.push_back (note);
    }

    // pattern
    uint32_t bits = 0;
    for (int s = 0; s < maxSteps; ++s)
        if (random.nextFloat() < density)
            bits |= (1u << s);

    // never leave the audible part of a track completely empty
    const uint32_t audibleMask = len >= 32 ? 0xffffffffu : ((1u << len) - 1u);
    if ((bits & audibleMask) == 0)
        bits |= (1u << random.nextInt (len));

    patterns[(size_t) track].store (bits);
}

void PolyRhythmProcessor::randomizeTrack (int track)
{
    const juce::ScopedLock sl (randomLock);
    pushUndo();

    std::vector<int> used;
    for (int t = 0; t < numTracks; ++t)
        if (t != track)
            used.push_back (getTrackNote (t));

    randomizeTrackInternal (track, used);
}

void PolyRhythmProcessor::randomizeAll()
{
    const juce::ScopedLock sl (randomLock);
    pushUndo();

    std::vector<int> used;
    for (int t = 0; t < numTracks; ++t)
        randomizeTrackInternal (t, used);
}

void PolyRhythmProcessor::clearTrack (int track)
{
    pushUndo();
    setPattern (track, 0);
}

//==============================================================================
PolyRhythmProcessor::Snapshot PolyRhythmProcessor::captureSnapshot() const
{
    Snapshot snap;
    for (int t = 0; t < numTracks; ++t)
    {
        snap.patterns[(size_t) t] = patterns[(size_t) t].load();
        int i = 0;
        for (auto* base : snapshotParams)
            snap.values[(size_t) t][(size_t) i++] = apvts.getRawParameterValue (id (base, t))->load();
    }
    return snap;
}

void PolyRhythmProcessor::restoreSnapshot (const Snapshot& snap)
{
    for (int t = 0; t < numTracks; ++t)
    {
        patterns[(size_t) t].store (snap.patterns[(size_t) t]);
        int i = 0;
        for (auto* base : snapshotParams)
            setParamValue (id (base, t), snap.values[(size_t) t][(size_t) i++]);
    }
}

void PolyRhythmProcessor::pushUndo()
{
    undoStack.push_back (captureSnapshot());
    if (undoStack.size() > 200)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void PolyRhythmProcessor::undo()
{
    if (undoStack.empty())
        return;
    redoStack.push_back (captureSnapshot());
    restoreSnapshot (undoStack.back());
    undoStack.pop_back();
}

void PolyRhythmProcessor::redo()
{
    if (redoStack.empty())
        return;
    undoStack.push_back (captureSnapshot());
    restoreSnapshot (redoStack.back());
    redoStack.pop_back();
}

//==============================================================================
void PolyRhythmProcessor::flushNoteOffs (juce::MidiBuffer& midi, int sampleOffset)
{
    for (auto& v : voices)
    {
        if (v.note >= 0)
            midi.addEvent (juce::MidiMessage::noteOff (v.channel, v.note), sampleOffset);
        v.note = -1;
        v.lastTrigPpq = -1.0e12;
    }
}

void PolyRhythmProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    generateBlock (buffer, midi);

    if (! midi.isEmpty())
    {
        const juce::SpinLock::ScopedTryLockType sl (virtualOutLock);
        if (sl.isLocked() && virtualOut != nullptr)
            virtualOut->sendBlockOfMessages (midi, juce::Time::getMillisecondCounterHiRes(), sampleRate);
    }
}

void PolyRhythmProcessor::generateBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    midi.clear();

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    bool synced = false, playing = false;
    double ppqStart = 0.0;
    double bpm = apvts.getRawParameterValue ("bpm")->load();

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            auto hostBpm = pos->getBpm();
            auto hostPpq = pos->getPpqPosition();
            if (hostBpm.hasValue() && hostPpq.hasValue() && *hostBpm > 0.0)
            {
                synced   = true;
                bpm      = *hostBpm;
                ppqStart = *hostPpq;
                playing  = pos->getIsPlaying();
            }
        }
    }

    if (! synced)
    {
        playing = apvts.getRawParameterValue ("play")->load() > 0.5f;
        if (playing && ! wasPlaying)
            internalPpq = 0.0;
        ppqStart = internalPpq;
    }

    hostSynced.store (synced);
    lastBpm.store (bpm);

    if (! playing)
    {
        if (wasPlaying)
            flushNoteOffs (midi, 0);
        wasPlaying = false;
        for (auto& s : currentStep)
            s.store (-1);
        return;
    }

    const double ppqPerSample = bpm / 60.0 / sampleRate;
    const double ppqEnd = ppqStart + numSamples * ppqPerSample;

    // transport started or position jumped (loop / locate) -> restart cleanly
    if (! wasPlaying)
    {
        flushNoteOffs (midi, 0);
    }
    else if (std::abs (ppqStart - lastPpqEnd) > ppqPerSample * 8.0 + 1.0e-6)
    {
        flushNoteOffs (midi, 0);
    }

    const int channel = (int) apvts.getRawParameterValue ("channel")->load();

    auto toSample = [&] (double ppq)
    {
        return juce::jlimit (0, numSamples - 1, (int) std::floor ((ppq - ppqStart) / ppqPerSample));
    };

    for (int t = 0; t < numTracks; ++t)
    {
        const int len      = getTrackLength (t);
        const double stepQ = getTrackStepQuarters (t);
        const int note     = getTrackNote (t);
        const int vel      = (int) apvts.getRawParameterValue (id ("vel", t))->load();
        const float gate   = apvts.getRawParameterValue (id ("gate", t))->load();
        const bool muted   = apvts.getRawParameterValue (id ("mute", t))->load() > 0.5f;
        const uint32_t bits = patterns[(size_t) t].load();

        auto& v = voices[(size_t) t];

        auto k = (juce::int64) std::ceil (ppqStart / stepQ - 1.0e-7);
        for (; (double) k * stepQ < ppqEnd; ++k)
        {
            const double tPpq = (double) k * stepQ;
            if (tPpq <= v.lastTrigPpq + 1.0e-7)
                continue; // already handled in a previous block

            v.lastTrigPpq = tPpq;

            if (v.note >= 0 && v.offPpq <= tPpq)
            {
                midi.addEvent (juce::MidiMessage::noteOff (v.channel, v.note), toSample (v.offPpq));
                v.note = -1;
            }

            const int stepIdx = (int) (((k % len) + len) % len);
            if (! muted && ((bits >> stepIdx) & 1u) != 0)
            {
                const int s = toSample (tPpq);
                if (v.note >= 0)
                {
                    midi.addEvent (juce::MidiMessage::noteOff (v.channel, v.note), s);
                    v.note = -1;
                }

                midi.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) vel), s);
                v.note    = note;
                v.channel = channel;
                v.offPpq  = tPpq + gate * stepQ;
            }
        }

        if (v.note >= 0 && v.offPpq < ppqEnd)
        {
            midi.addEvent (juce::MidiMessage::noteOff (v.channel, v.note), toSample (v.offPpq));
            v.note = -1;
        }

        const auto cur = (juce::int64) std::floor ((ppqEnd - ppqPerSample) / stepQ);
        currentStep[(size_t) t].store ((int) (((cur % len) + len) % len));
    }

    if (! synced)
        internalPpq = ppqEnd;

    lastPpqEnd = ppqEnd;
    wasPlaying = true;
}

//==============================================================================
juce::MidiFile PolyRhythmProcessor::createMidiLoop (int& totalBarsOut, bool& wasCapped) const
{
    const juce::int64 barTicks = 4 * ticksPerQuarter;
    const int exportIdx = (int) apvts.getRawParameterValue ("exportLen")->load();

    wasCapped = false;
    juce::int64 total = barTicks;

    if (exportIdx == 0)
    {
        // smallest bar-aligned length after which every track repeats exactly
        juce::int64 l = barTicks;
        for (int t = 0; t < numTracks; ++t)
        {
            if (apvts.getRawParameterValue (id ("mute", t))->load() > 0.5f)
                continue;

            const auto stepTicks = (juce::int64) juce::roundToInt (getTrackStepQuarters (t) * ticksPerQuarter);
            l = std::lcm (l, stepTicks * getTrackLength (t));
            if (l > barTicks * maxExportBars)
            {
                wasCapped = true;
                break;
            }
        }
        total = wasCapped ? barTicks * 16 : l;
    }
    else
    {
        total = barTicks * (juce::int64) (1 << (exportIdx - 1));
    }

    totalBarsOut = (int) (total / barTicks);

    const int channel = (int) apvts.getRawParameterValue ("channel")->load();
    const double bpm = lastBpm.load();

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::textMetaEvent (3, "PolyRhythm32 Loop"), 0.0);
    seq.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / juce::jmax (1.0, bpm))), 0.0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);

    for (int t = 0; t < numTracks; ++t)
    {
        if (apvts.getRawParameterValue (id ("mute", t))->load() > 0.5f)
            continue;

        const int len  = getTrackLength (t);
        const int note = getTrackNote (t);
        const int vel  = (int) apvts.getRawParameterValue (id ("vel", t))->load();
        const float gate = apvts.getRawParameterValue (id ("gate", t))->load();
        const uint32_t bits = patterns[(size_t) t].load();
        const auto stepTicks = (juce::int64) juce::roundToInt (getTrackStepQuarters (t) * ticksPerQuarter);
        const auto gateTicks = juce::jmax ((juce::int64) 1, (juce::int64) juce::roundToInt (gate * (float) stepTicks));

        for (juce::int64 k = 0; k * stepTicks < total; ++k)
        {
            if (((bits >> (int) (k % len)) & 1u) == 0)
                continue;

            const auto on  = k * stepTicks;
            const auto off = juce::jmin (total, on + gateTicks);
            seq.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) vel), (double) on);
            seq.addEvent (juce::MidiMessage::noteOff (channel, note), (double) off);
        }
    }

    seq.addEvent (juce::MidiMessage::endOfTrack(), (double) total);
    seq.sort();
    seq.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (ticksPerQuarter);
    file.addTrack (seq);
    return file;
}

bool PolyRhythmProcessor::writeMidiLoop (const juce::File& file, juce::String& infoOut) const
{
    int bars = 0;
    bool capped = false;
    auto midiFile = createMidiLoop (bars, capped);

    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk())
    {
        infoOut = "Fehler: Datei konnte nicht geschrieben werden";
        return false;
    }

    midiFile.writeTo (out, 1);
    out.flush();

    infoOut = juce::String (bars) + (bars == 1 ? " Takt" : " Takte");
    if (capped)
        infoOut += " (Loop > " + juce::String (maxExportBars) + " Takte, gekuerzt)";
    return true;
}

//==============================================================================
void PolyRhythmProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto pat = state.getOrCreateChildWithName ("Patterns", nullptr);
    for (int t = 0; t < numTracks; ++t)
        pat.setProperty ("p" + juce::String (t), (juce::int64) patterns[(size_t) t].load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PolyRhythmProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.hasType (apvts.state.getType()))
            return;

        auto pat = tree.getChildWithName ("Patterns");
        if (pat.isValid())
            for (int t = 0; t < numTracks; ++t)
            {
                const auto key = "p" + juce::String (t);
                if (pat.hasProperty (key))
                    patterns[(size_t) t].store ((uint32_t) (juce::int64) pat.getProperty (key));
            }

        apvts.replaceState (tree);
    }
}

juce::AudioProcessorEditor* PolyRhythmProcessor::createEditor()
{
    return new PolyRhythmEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PolyRhythmProcessor();
}
