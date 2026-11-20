#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using Range = juce::NormalisableRange<float>;

    Range expRange (float a, float b)
    {
        return Range (a, b,
            [] (float s, float e, float n) { return s * std::pow (e / s, n); },
            [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
            nullptr);
    }
    Range powRange (float a, float b, float k)
    {
        return Range (a, b,
            [k] (float s, float e, float n) { return s + (e - s) * std::pow (n, k); },
            [k] (float s, float e, float v) { return std::pow (juce::jlimit (0.0f, 1.0f, (v - s) / (e - s)), 1.0f / k); },
            nullptr);
    }
    juce::String fms (float v) { return v < 1000 ? juce::String (juce::roundToInt (v)) + " ms" : juce::String (v / 1000.0f, 2) + " s"; }
    juce::String fhz (float v) { return v >= 1000 ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }
    juce::String fpct (float v) { return juce::String (juce::roundToInt (v * 100)) + "%"; }

    // Standard-Slots wie in der Browser-Version: Clean Break, Aah, Warm Pad, Tape Keys, FM Bell, Acid Line
    constexpr int defaultSlotPresets[] = { 0, 25, 50, 75, 100, 125 };
}

const juce::StringArray& MikrokornProcessor::presetParamIds()
{
    static const juce::StringArray ids { "tune", "start", "end", "grain", "shift", "smooth", "crush", "srate",
                                         "attack", "release", "cutoff", "reso", "drive", "vol", "loop", "reverse" };
    return ids;
}

juce::AudioProcessorValueTreeState::ParameterLayout MikrokornProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto add = [&p] (const char* id, const char* name, Range r, float def, std::function<juce::String (float, int)> fmt)
    {
        p.push_back (std::make_unique<F> (juce::ParameterID { id, 1 }, name, r, def, A().withStringFromValueFunction (std::move (fmt))));
    };

    add ("tune",    "Pitch",   Range (-24.0f, 24.0f, 1.0f), 0.0f,  [] (float v, int) { return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v)) + " st"; });
    add ("start",   "Start",   Range (0.0f, 1.0f), 0.0f,           [] (float v, int) { return juce::String (v * 100.0f, 1) + "%"; });
    add ("end",     "Ende",    Range (0.0f, 1.0f), 1.0f,           [] (float v, int) { return juce::String (v * 100.0f, 1) + "%"; });
    add ("grain",   "Grain",   powRange (0.0f, 500.0f, 2.0f), 0.0f, [] (float v, int) { return v < 2 ? juce::String ("OFF") : juce::String (v, v < 10 ? 1 : 0) + " ms"; });
    add ("shift",   "Shift",   Range (-2.0f, 2.0f), 1.0f,          [] (float v, int) { return juce::String (v, 2) + "x"; });
    add ("smooth",  "Smooth",  Range (0.0f, 1.0f), 0.3f,           [] (float v, int) { return fpct (v); });
    add ("crush",   "Crush",   Range (0.0f, 15.0f, 1.0f), 0.0f,    [] (float v, int) { return v < 0.5f ? juce::String ("OFF") : juce::String (16 - juce::roundToInt (v)) + " bit"; });
    add ("srate",   "Rate",    expRange (400.0f, 48000.0f), 48000.0f, [] (float v, int) { return v >= 47500 ? juce::String ("OFF") : fhz (v); });
    add ("attack",  "Attack",  powRange (0.0f, 2000.0f, 3.0f), 2.0f, [] (float v, int) { return fms (v); });
    add ("release", "Release", powRange (0.0f, 3000.0f, 3.0f), 150.0f, [] (float v, int) { return fms (v); });
    add ("cutoff",  "Filter",  expRange (60.0f, 18000.0f), 18000.0f, [] (float v, int) { return v >= 17900 ? juce::String ("OFF") : fhz (v); });
    add ("reso",    "Reso",    expRange (0.5f, 18.0f), 0.7f,       [] (float v, int) { return juce::String (v, 1); });
    add ("drive",   "Drive",   Range (0.0f, 1.0f), 0.0f,           [] (float v, int) { return fpct (v); });
    add ("vol",     "Volume",  Range (0.0f, 1.0f), 0.8f,           [] (float v, int) { return fpct (v); });

    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "loop", 1 },    "Loop",    true));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "reverse", 1 }, "Reverse", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "legato", 1 },  "Legato",  false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "hold", 1 },    "Hold",    false));
    return { p.begin(), p.end() };
}

MikrokornProcessor::MikrokornProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pTune = raw ("tune"); pStart = raw ("start"); pEnd = raw ("end"); pGrain = raw ("grain"); pShift = raw ("shift");
    pSmooth = raw ("smooth"); pCrush = raw ("crush"); pSrate = raw ("srate"); pAttack = raw ("attack"); pRelease = raw ("release");
    pCutoff = raw ("cutoff"); pReso = raw ("reso"); pDrive = raw ("drive"); pVol = raw ("vol");
    pLoop = raw ("loop"); pReverse = raw ("reverse"); pLegato = raw ("legato"); pHold = raw ("hold");

    for (auto& id : presetParamIds())
        apvts.addParameterListener (id, this);

    // Slots mit den Standard-Presets füllen
    for (int i = 0; i < numSlots; ++i)
    {
        auto pr = library.getPreset (defaultSlotPresets[i]);
        auto& s = slots[(size_t) i];
        s.sampleRef = pr.sampleRef; s.name = pr.name; s.preset = defaultSlotPresets[i]; s.values = pr.values;
    }
    setActiveSample (library.find (slots[0].sampleRef));
    applyValues (slots[0].values);
    currentEdited = false;

    startTimer (1000);
}

MikrokornProcessor::~MikrokornProcessor()
{
    stopTimer();
    for (auto& id : presetParamIds())
        apvts.removeParameterListener (id, this);
}

bool MikrokornProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void MikrokornProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    filter.prepare (spec);
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.reset();
    gainSmooth.reset (sampleRate, 0.02);
    gainSmooth.setCurrentAndTargetValue (pVol->load());
    monoSize = std::max (samplesPerBlock, 512) * 2;
    mono.allocate ((size_t) monoSize, true);
    engineSample = nullptr;   // erzwingt neues Setzen des Samples
}

void MikrokornProcessor::readParams (GrainParams& p) const
{
    p.tune = pTune->load(); p.start = pStart->load(); p.end = pEnd->load(); p.grain = pGrain->load();
    p.shift = pShift->load(); p.smooth = pSmooth->load(); p.bits = 16.0f - std::round (pCrush->load());
    p.srate = pSrate->load(); p.attack = pAttack->load(); p.release = pRelease->load();
    p.loop = pLoop->load() > 0.5f; p.reverse = pReverse->load() > 0.5f;
}

void MikrokornProcessor::handleMidi (const juce::MidiMessage& m, const GrainParams& p)
{
    const bool legato = pLegato->load() > 0.5f;
    const bool hold = pHold->load() > 0.5f;
    auto removeNote = [this] (int n)
    {
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i] == n)
            {
                for (int j = i; j < numHeld - 1; ++j) held[(size_t) j] = held[(size_t) j + 1];
                --numHeld;
                return true;
            }
        return false;
    };

    if (m.isNoteOn())
    {
        const int n = m.getNoteNumber();
        const float vel = 0.25f + 0.75f * m.getFloatVelocity();
        removeNote (n);
        const bool retrig = ! legato || numHeld == 0;
        if (numHeld < 128) held[(size_t) numHeld++] = n;
        lastVel = vel;
        engine.noteOn (n, vel, retrig, p);
    }
    else if (m.isNoteOff())
    {
        if (! removeNote (m.getNoteNumber())) return;
        if (numHeld > 0) engine.noteOn (held[(size_t) numHeld - 1], lastVel, ! legato, p);
        else if (! hold) engine.noteOff();
    }
    else if (m.isAllSoundOff())
    {
        numHeld = 0; engine.kill();
    }
    else if (m.isAllNotesOff())
    {
        numHeld = 0; engine.noteOff();
    }
    else if (m.isPitchWheel())
    {
        engine.setBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f);
    }
}

void MikrokornProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    {
        const juce::SpinLock::ScopedTryLockType lock (sampleLock);
        if (lock.isLocked() && audioSample != activeSample)
            audioSample = activeSample;
    }
    if (audioSample == nullptr) return;
    if (audioSample.get() != engineSample)
    {
        engine.setSample (audioSample->buffer.getReadPointer (0), audioSample->buffer.getNumSamples(), audioSample->sampleRate);
        engineSample = audioSample.get();
        numHeld = 0;
    }

    if (n > monoSize) { monoSize = n; mono.allocate ((size_t) n, true); }

    GrainParams gp;
    readParams (gp);

    const bool hold = pHold->load() > 0.5f;
    if (prevHold && ! hold && numHeld == 0) engine.noteOff();
    prevHold = hold;

    int pos = 0;
    for (const auto md : midi)
    {
        const int t = juce::jlimit (0, n, md.samplePosition);
        if (t > pos) { engine.process (mono.get() + pos, t - pos, gp); pos = t; }
        handleMidi (md.getMessage(), gp);
    }
    if (pos < n) engine.process (mono.get() + pos, n - pos, gp);

    // Ausgang: Filter -> Drive -> Volume
    const double sr = getSampleRate() > 0 ? getSampleRate() : 48000.0;
    filter.setCutoffFrequency (std::min (pCutoff->load(), (float) (sr * 0.45)));
    filter.setResonance (pReso->load());
    const float drive = pDrive->load();
    const float k = 1.0f + drive * 30.0f, tk = std::tanh (k);
    gainSmooth.setTargetValue (pVol->load() * (1.0f - drive * 0.45f));

    auto* L = buffer.getWritePointer (0);
    for (int i = 0; i < n; ++i)
    {
        float x = filter.processSample (0, mono[i]);
        if (drive > 0.005f) x = std::tanh (x * k) / tk;
        L[i] = x * gainSmooth.getNextValue();
    }
    for (int c = 1; c < buffer.getNumChannels(); ++c)
        buffer.copyFrom (c, 0, buffer, 0, 0, n);

    uiPos = engine.uiPos (gp);
    uiGrain = engine.uiGrain (gp);
    uiActive = engine.isActive();
    uiEnv = engine.envelope();
}

// ---------------- Parameter / Slots / Presets ----------------

void MikrokornProcessor::parameterChanged (const juce::String&, float)
{
    if (! suppressEdits.load() && ! currentEdited.exchange (true))
        bump();
}

std::map<juce::String, float> MikrokornProcessor::captureValues() const
{
    std::map<juce::String, float> v;
    for (auto& id : presetParamIds())
        v[id] = apvts.getRawParameterValue (id)->load();
    return v;
}

void MikrokornProcessor::applyValues (const std::map<juce::String, float>& values)
{
    suppressEdits = true;
    for (auto& id : presetParamIds())
        if (auto* p = apvts.getParameter (id))
        {
            auto it = values.find (id);
            p->setValueNotifyingHost (it != values.end() ? p->convertTo0to1 (it->second) : p->getDefaultValue());
        }
    suppressEdits = false;
}

void MikrokornProcessor::setActiveSample (SamplePtr s)
{
    if (s == nullptr) s = library.find ("factory:drums");
    const juce::SpinLock::ScopedLockType lock (sampleLock);
    if (activeSample) graveyard.push_back (activeSample);
    activeSample = std::move (s);
}

SamplePtr MikrokornProcessor::getCurrentSample() const
{
    const juce::SpinLock::ScopedLockType lock (const_cast<juce::SpinLock&> (sampleLock));
    return activeSample;
}

void MikrokornProcessor::timerCallback()
{
    std::vector<SamplePtr> dead;
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        for (auto it = graveyard.begin(); it != graveyard.end();)
        {
            if (it->use_count() <= 1) { dead.push_back (std::move (*it)); it = graveyard.erase (it); }
            else ++it;
        }
    }
    dead.clear();   // Freigabe außerhalb des Locks, auf dem Message-Thread
}

void MikrokornProcessor::selectSlot (int index)
{
    index = juce::jlimit (0, numSlots - 1, index);
    auto& old = slots[(size_t) currentSlot];
    old.values = captureValues();
    old.edited = currentEdited;

    currentSlot = index;
    auto& s = slots[(size_t) index];
    setActiveSample (library.find (s.sampleRef));
    applyValues (s.values);
    currentEdited = s.edited;
    bump();
}

void MikrokornProcessor::loadPreset (int index)
{
    const int count = library.getNumPresets();
    if (count == 0) return;
    index = ((index % count) + count) % count;
    auto pr = library.getPreset (index);
    auto& s = slots[(size_t) currentSlot];
    auto sample = library.find (pr.sampleRef);
    s.sampleRef = sample ? pr.sampleRef : juce::String ("factory:drums");
    s.name = pr.name;
    s.preset = index;
    s.edited = false;
    setActiveSample (sample);
    applyValues (pr.values);
    currentEdited = false;
    bump();
}

void MikrokornProcessor::stepPreset (int delta)
{
    const int cur = getCurrentPreset();
    loadPreset (cur < 0 ? 0 : cur + delta);
}

int MikrokornProcessor::saveCurrentAsPreset (const juce::String& name)
{
    auto& s = slots[(size_t) currentSlot];
    const int idx = library.saveUserPreset (name, s.sampleRef, captureValues());
    if (idx >= 0)
    {
        s.preset = idx; s.name = name; s.edited = false;
        currentEdited = false;
    }
    bump();
    return idx;
}

bool MikrokornProcessor::deleteCurrentPreset()
{
    auto& s = slots[(size_t) currentSlot];
    const int idx = s.preset;
    if (idx < library.getNumFactoryPresets()) return false;
    const auto file = library.getPreset (idx).file;
    // Datei-Referenzen der anderen Slots merken, damit ihre Nummern nach dem Löschen stimmen
    std::array<juce::File, numSlots> files;
    for (int i = 0; i < numSlots; ++i)
        if (slots[(size_t) i].preset >= library.getNumFactoryPresets())
            files[(size_t) i] = library.getPreset (slots[(size_t) i].preset).file;

    if (! library.deleteUserPreset (idx)) return false;
    for (int i = 0; i < numSlots; ++i)
        if (files[(size_t) i] != juce::File())
            slots[(size_t) i].preset = files[(size_t) i] == file ? -1 : library.findUserPresetByFile (files[(size_t) i]);
    bump();
    return true;
}

void MikrokornProcessor::useSample (const juce::String& ref)
{
    auto sample = library.find (ref);
    if (sample == nullptr) return;
    auto& s = slots[(size_t) currentSlot];
    s.sampleRef = ref;
    s.name = SampleLibrary::displayName (ref);
    s.preset = -1;
    setActiveSample (sample);
    suppressEdits = true;
    if (auto* p = apvts.getParameter ("start")) p->setValueNotifyingHost (0.0f);
    if (auto* p = apvts.getParameter ("end"))   p->setValueNotifyingHost (1.0f);
    suppressEdits = false;
    currentEdited = false;
    bump();
}

bool MikrokornProcessor::importSamples (const juce::Array<juce::File>& files, juce::String& error)
{
    SamplePtr last;
    for (auto& f : files)
        if (auto s = library.importFile (f, error))
            last = s;
    if (last) useSample (last->ref);
    bump();
    return last != nullptr;
}

bool MikrokornProcessor::deleteUserSample (const juce::String& ref)
{
    std::array<juce::File, numSlots> files;
    for (int i = 0; i < numSlots; ++i)
        if (slots[(size_t) i].preset >= library.getNumFactoryPresets())
            files[(size_t) i] = library.getPreset (slots[(size_t) i].preset).file;

    const bool ok = library.deleteUserSample (ref);

    for (int i = 0; i < numSlots; ++i)
    {
        auto& s = slots[(size_t) i];
        if (files[(size_t) i] != juce::File())
            s.preset = library.findUserPresetByFile (files[(size_t) i]);
        if (s.sampleRef == ref)
        {
            if (i == currentSlot) loadPreset (defaultSlotPresets[i]);
            else
            {
                auto pr = library.getPreset (defaultSlotPresets[i]);
                s.sampleRef = pr.sampleRef; s.name = pr.name; s.preset = defaultSlotPresets[i]; s.values = pr.values; s.edited = false;
            }
        }
    }
    bump();
    return ok;
}

void MikrokornProcessor::randomize()
{
    juce::Random r;
    auto R = [&r] { return r.nextFloat(); };
    const float a = R() * 0.7f;
    std::map<juce::String, float> v = captureValues();
    v["start"] = a;
    v["end"] = std::min (1.0f, a + 0.05f + R() * 0.5f);
    v["grain"] = R() < 0.2f ? 0.0f : 5.0f + R() * R() * 300.0f;
    v["shift"] = (R() * 2.0f - 0.6f) * (R() < 0.3f ? 0.0f : 1.0f);
    v["smooth"] = R();
    v["crush"] = R() < 0.5f ? 0.0f : (float) juce::roundToInt (16.0f - (3.0f + R() * 10.0f));
    v["srate"] = R() < 0.5f ? 48000.0f : 1500.0f + R() * R() * 20000.0f;
    v["tune"] = R() < 0.6f ? 0.0f : (float) juce::roundToInt ((R() * 2.0f - 1.0f) * 12.0f);
    v["reverse"] = R() < 0.2f ? 1.0f : 0.0f;
    applyValues (v);
    currentEdited = true;
    bump();
}

void MikrokornProcessor::initSlot()
{
    applyValues ({});
    currentEdited = true;
    bump();
}

// ---------------- Programme (Host-Presets) ----------------

int MikrokornProcessor::getNumPrograms() { return std::max (1, library.getNumFactoryPresets()); }

int MikrokornProcessor::getCurrentProgram()
{
    const int p = getCurrentPreset();
    return (p >= 0 && p < library.getNumFactoryPresets()) ? p : 0;
}

void MikrokornProcessor::setCurrentProgram (int index)
{
    if (index == getCurrentPreset()) return;
    if (juce::MessageManager::existsAndIsCurrentThread())
        loadPreset (index);
    else
        juce::MessageManager::callAsync ([safe = juce::WeakReference<MikrokornProcessor> (this), index]
        {
            if (auto* p = safe.get()) p->loadPreset (index);
        });
}

const juce::String MikrokornProcessor::getProgramName (int index)
{
    return library.getPreset (index).name;
}

// ---------------- State ----------------

void MikrokornProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto& cur = slots[(size_t) currentSlot];
    cur.values = captureValues();
    cur.edited = currentEdited;

    juce::ValueTree root ("MIKROKORN");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("slot", currentSlot, nullptr);
    root.setProperty ("legato", pLegato->load(), nullptr);
    root.setProperty ("hold", pHold->load(), nullptr);
    for (auto& s : slots)
    {
        juce::ValueTree t ("SLOT");
        t.setProperty ("sample", s.sampleRef, nullptr);
        t.setProperty ("name", s.name, nullptr);
        t.setProperty ("preset", s.preset, nullptr);
        t.setProperty ("edited", s.edited, nullptr);
        if (s.preset >= library.getNumFactoryPresets())
            t.setProperty ("presetFile", library.getPreset (s.preset).file.getFileName(), nullptr);
        for (auto& [k, v] : s.values) t.setProperty (k, v, nullptr);
        root.appendChild (t, nullptr);
    }
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void MikrokornProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("MIKROKORN")) return;

    for (int i = 0; i < numSlots && i < root.getNumChildren(); ++i)
    {
        auto t = root.getChild (i);
        auto& s = slots[(size_t) i];
        s.sampleRef = t.getProperty ("sample", "factory:drums").toString();
        s.name = t.getProperty ("name").toString();
        s.preset = (int) t.getProperty ("preset", -1);
        s.edited = (bool) t.getProperty ("edited", false);
        auto pf = t.getProperty ("presetFile").toString();
        if (pf.isNotEmpty())
        {
            const int idx = library.findUserPresetByFile (SampleLibrary::presetsDir().getChildFile (pf));
            s.preset = idx;
        }
        else if (s.preset >= library.getNumFactoryPresets()) s.preset = -1;
        s.values.clear();
        for (auto& id : presetParamIds())
            if (t.hasProperty (id)) s.values[id] = (float) t.getProperty (id);
        if (library.find (s.sampleRef) == nullptr)
            s.sampleRef = "factory:drums";
    }

    currentSlot = juce::jlimit (0, numSlots - 1, (int) root.getProperty ("slot", 0));
    auto& s = slots[(size_t) currentSlot];
    setActiveSample (library.find (s.sampleRef));
    applyValues (s.values);
    currentEdited = s.edited;
    if (auto* p = apvts.getParameter ("legato")) p->setValueNotifyingHost ((float) root.getProperty ("legato", 0.0f) > 0.5f ? 1.0f : 0.0f);
    if (auto* p = apvts.getParameter ("hold"))   p->setValueNotifyingHost ((float) root.getProperty ("hold", 0.0f) > 0.5f ? 1.0f : 0.0f);
    bump();
}

juce::AudioProcessorEditor* MikrokornProcessor::createEditor() { return new MikrokornEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MikrokornProcessor(); }
