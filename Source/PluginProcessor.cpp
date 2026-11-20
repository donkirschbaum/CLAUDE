#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;
using Range = juce::NormalisableRange<float>;

namespace
{
Range logRange (float lo, float hi)
{
    return Range (lo, hi,
        [] (float s, float e, float n) { return s * std::pow (e / s, n); },
        [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); });
}

juce::ParameterID pid (const juce::String& id) { return { id, 1 }; }
juce::String idx (const char* base, int i)     { return juce::String (base) + juce::String (i); }
}

juce::AudioProcessorValueTreeState::ParameterLayout DroneAchtProcessor::createLayout()
{
    const drone::Params d;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = juce::AudioParameterFloatAttributes().withLabel ("%")
                 .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100)); });
    auto hz  = juce::AudioParameterFloatAttributes().withLabel ("Hz");
    auto sec = juce::AudioParameterFloatAttributes().withLabel ("ms")
                 .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 1000)); });
    auto st  = juce::AudioParameterFloatAttributes().withLabel ("HT");

    l.add (std::make_unique<APF> (pid ("vol"),     "Volume",   Range (0, 1), d.vol, pct));
    l.add (std::make_unique<APF> (pid ("drive"),   "Drive",    Range (0, 1), d.drive, pct));
    l.add (std::make_unique<APF> (pid ("vib"),     "Vibrato",  Range (0, 1), d.vib, pct));
    l.add (std::make_unique<APF> (pid ("vibRate"), "Vib-Rate", logRange (.1f, 12.f), d.vibRate, hz));

    l.add (std::make_unique<APF> (pid ("lfoA"), "LFO Rate A", logRange (.02f, 30.f), d.lfoA, hz));
    l.add (std::make_unique<APF> (pid ("lfoB"), "LFO Rate B", logRange (.02f, 30.f), d.lfoB, hz));
    l.add (std::make_unique<APC> (pid ("lfoMode"), "LFO Verknuepfung", juce::StringArray { "A+B", juce::String::fromUTF8 ("A\xc3\x97" "B") }, d.lfoMode));
    l.add (std::make_unique<APC> (pid ("lfoWave"), "LFO Form", juce::StringArray { "Dreieck", "Rechteck" }, d.lfoWave));

    l.add (std::make_unique<APF> (pid ("t1"),   "Delay Zeit L", logRange (.02f, 1.6f), d.t1, sec));
    l.add (std::make_unique<APF> (pid ("t2"),   "Delay Zeit R", logRange (.02f, 1.6f), d.t2, sec));
    l.add (std::make_unique<APF> (pid ("fb"),   "Delay Feedback", Range (0, .95f), d.fb, pct));
    l.add (std::make_unique<APF> (pid ("dmod"), "Delay Mod", Range (0, 1), d.dmod, pct));
    l.add (std::make_unique<APF> (pid ("dmix"), "Delay Mix", Range (0, 1), d.dmix, pct));

    for (int q = 0; q < 2; ++q)
    {
        const juce::String n = q == 0 ? "Quad A " : "Quad B ";
        l.add (std::make_unique<APF> (pid (idx ("hold", q)),  n + "Hold",  Range (0, 1), d.hold[q], pct));
        l.add (std::make_unique<APF> (pid (idx ("pitch", q)), n + "Pitch", Range (-12, 12), d.pitch[q], st));
        l.add (std::make_unique<APC> (pid (idx ("src", q)),   n + "Mod-Quelle",
                                      juce::StringArray { "Paar", "Nachbar", "LFO" }, d.src[q]));
    }
    for (int p = 0; p < 4; ++p)
    {
        const juce::String n = "Paar " + juce::String (p * 2 + 1) + "-" + juce::String (p * 2 + 2) + " ";
        l.add (std::make_unique<APF> (pid (idx ("mod", p)),   n + "Mod",   Range (0, 1), d.mod[p], pct));
        l.add (std::make_unique<APF> (pid (idx ("sharp", p)), n + "Sharp", Range (0, 1), d.sharp[p], pct));
    }
    for (int i = 0; i < drone::numVoices; ++i)
    {
        const juce::String n = "Stimme " + juce::String (i + 1) + " ";
        l.add (std::make_unique<APF> (pid (idx ("tune", i)), n + "Tune", Range (-12, 12), d.tune[i], st));
        l.add (std::make_unique<APB> (pid (idx ("fast", i)), n + "Schnell", d.fast[i]));
    }
    return l;
}

//==============================================================================
DroneAchtProcessor::DroneAchtProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DroneAcht", createLayout())
{
    for (int i = 0; i < drone::numVoices; ++i) { padTouch[(size_t) i].store (false); keyTouch[(size_t) i].store (false); }

    auto r = [this] (const juce::String& id) { auto* p = apvts.getRawParameterValue (id); jassert (p != nullptr); return p; };
    raw.vol = r ("vol"); raw.drive = r ("drive"); raw.vib = r ("vib"); raw.vibRate = r ("vibRate");
    raw.lfoA = r ("lfoA"); raw.lfoB = r ("lfoB"); raw.lfoMode = r ("lfoMode"); raw.lfoWave = r ("lfoWave");
    raw.t1 = r ("t1"); raw.t2 = r ("t2"); raw.fb = r ("fb"); raw.dmod = r ("dmod"); raw.dmix = r ("dmix");
    for (int q = 0; q < 2; ++q) { raw.hold[q] = r (idx ("hold", q)); raw.pitch[q] = r (idx ("pitch", q)); raw.src[q] = r (idx ("src", q)); }
    for (int p = 0; p < 4; ++p) { raw.mod[p] = r (idx ("mod", p)); raw.sharp[p] = r (idx ("sharp", p)); }
    for (int i = 0; i < drone::numVoices; ++i) { raw.tune[i] = r (idx ("tune", i)); raw.fast[i] = r (idx ("fast", i)); }

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            params.push_back (rp);

    lastCC.fill (0);
    loadDefaultMidiMap();
    startTimerHz (30);
}

DroneAchtProcessor::~DroneAchtProcessor() { stopTimer(); }

bool DroneAchtProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void DroneAchtProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    engine.reset();
}

void DroneAchtProcessor::readParams (drone::Params& p) const
{
    p.vol = raw.vol->load(); p.drive = raw.drive->load(); p.vib = raw.vib->load(); p.vibRate = raw.vibRate->load();
    p.lfoA = raw.lfoA->load(); p.lfoB = raw.lfoB->load();
    p.lfoMode = (int) raw.lfoMode->load(); p.lfoWave = (int) raw.lfoWave->load();
    p.t1 = raw.t1->load(); p.t2 = raw.t2->load(); p.fb = raw.fb->load(); p.dmod = raw.dmod->load(); p.dmix = raw.dmix->load();
    for (int q = 0; q < 2; ++q) { p.hold[q] = raw.hold[q]->load(); p.pitch[q] = raw.pitch[q]->load(); p.src[q] = (int) raw.src[q]->load(); }
    for (int i = 0; i < 4; ++i) { p.mod[i] = raw.mod[i]->load(); p.sharp[i] = raw.sharp[i]->load(); }
    for (int i = 0; i < drone::numVoices; ++i) { p.tune[i] = raw.tune[i]->load(); p.fast[i] = raw.fast[i]->load() > .5f; }
}

//==============================================================================
void DroneAchtProcessor::handleCC (int cc, int value)
{
    const int learn = learnTarget.load();
    if (learn >= 0)
    {
        for (auto& m : ccMap)
            if (m.load() == learn) m.store (-1);
        ccMap[(size_t) cc].store (learn);
        learnTarget.store (-1);
        lastCC[(size_t) cc] = value;
        bumpMap();
        return;
    }

    const int target = ccMap[(size_t) cc].load();
    const int prev = lastCC[(size_t) cc];
    lastCC[(size_t) cc] = value;
    if (target < 0) return;

    if (target >= actionBase)
    {
        const int a = target - actionBase;
        if (a < numActions && prev < 64 && value >= 64)
            pendingActions.fetch_or (1u << a);
        return;
    }
    if (target < numParams())
    {
        auto* p = params[(size_t) target];
        const float norm = (float) value / 127.f;
        if (std::abs (p->getValue() - norm) > 1.0e-4f)
            p->setValueNotifyingHost (norm);
    }
}

void DroneAchtProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isController())
        {
            const int cc = m.getControllerNumber();
            if (cc == 120 || cc == 123) midiCount.fill (0);   // All Sound / All Notes Off
            else handleCC (cc, m.getControllerValue());
            continue;
        }
        if (! (m.isNoteOn() || m.isNoteOff())) continue;
        for (int i = 0; i < drone::numVoices; ++i)
        {
            if (m.getNoteNumber() != midiNotes[i]) continue;
            if (m.isNoteOn())       ++midiCount[(size_t) i];
            else                    midiCount[(size_t) i] = juce::jmax (0, midiCount[(size_t) i] - 1);
        }
    }

    drone::Params p;
    readParams (p);
    bool touching[drone::numVoices];
    for (int i = 0; i < drone::numVoices; ++i)
        touching[i] = padTouch[(size_t) i].load() || keyTouch[(size_t) i].load() || midiCount[(size_t) i] > 0;

    if (buffer.getNumChannels() < 2 || numSamples == 0) return;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    engine.process (p, touching, L, R, numSamples);

    int w = scopeWrite.load (std::memory_order_relaxed);
    for (int n = 0; n < numSamples; ++n)
    {
        scopeBuf[(size_t) w] = (L[n] + R[n]) * .5f;
        w = (w + 1) & (scopeSize - 1);
    }
    scopeWrite.store (w, std::memory_order_release);
}

void DroneAchtProcessor::timerCallback()
{
    const uint32_t pending = pendingActions.exchange (0);
    for (int a = 0; a < numActions; ++a)
        if (pending & (1u << a))
            performAction (a);
}

//==============================================================================
// MIDI-Zuordnung

const std::vector<int>& DroneAchtProcessor::defaultCCs()
{
    // Freie Controller nach MIDI-Spezifikation: 14–31, 70–90, 102–119 (57 Stück)
    static const std::vector<int> ccs = []
    {
        std::vector<int> v;
        for (int c = 14;  c <= 31;  ++c) v.push_back (c);
        for (int c = 70;  c <= 90;  ++c) v.push_back (c);
        for (int c = 102; c <= 119; ++c) v.push_back (c);
        return v;
    }();
    return ccs;
}

void DroneAchtProcessor::loadDefaultMidiMap()
{
    for (auto& m : ccMap) m.store (-1);
    const auto& ccs = defaultCCs();
    size_t slot = 0;
    for (int t = 0; t < numParams() && slot < ccs.size(); ++t)
        ccMap[(size_t) ccs[slot++]].store (t);
    for (int a = 0; a < numActions && slot < ccs.size(); ++a)
        ccMap[(size_t) ccs[slot++]].store (actionBase + a);
    learnTarget.store (-1);
    bumpMap();
}

void DroneAchtProcessor::clearMidiMap()
{
    for (auto& m : ccMap) m.store (-1);
    learnTarget.store (-1);
    bumpMap();
}

void DroneAchtProcessor::clearTarget (int target)
{
    for (auto& m : ccMap)
        if (m.load() == target) m.store (-1);
    if (learnTarget.load() == target) learnTarget.store (-1);
    bumpMap();
}

int DroneAchtProcessor::targetForParam (const juce::String& id) const
{
    for (int i = 0; i < numParams(); ++i)
        if (params[(size_t) i]->getParameterID() == id) return i;
    return -1;
}

int DroneAchtProcessor::ccForTarget (int target) const
{
    for (int c = 0; c < 128; ++c)
        if (ccMap[(size_t) c].load() == target) return c;
    return -1;
}

juce::String DroneAchtProcessor::targetName (int target) const
{
    if (target >= actionBase) return actionName (target - actionBase);
    if (target >= 0 && target < numParams()) return params[(size_t) target]->getName (64);
    return {};
}

juce::String DroneAchtProcessor::targetKey (int target) const
{
    if (target >= actionBase) return "a:" + juce::String (target - actionBase);
    return "p:" + params[(size_t) target]->getParameterID();
}

int DroneAchtProcessor::targetFromKey (const juce::String& key) const
{
    if (key.startsWith ("a:"))
    {
        const int a = key.substring (2).getIntValue();
        return a >= 0 && a < numActions ? actionBase + a : -1;
    }
    if (key.startsWith ("p:")) return targetForParam (key.substring (2));
    return -1;
}

//==============================================================================
// Aktionen

juce::String DroneAchtProcessor::actionName (int a)
{
    switch (a)
    {
        case actAll:    return "Zufall: alles";
        case actMutate: return "Mutieren";
        case actChaos:  return "Chaos";
        case actTune:   return "Zufall: Stimmung";
        case actMaster: return "Zufall: Master";
        case actLfo:    return "Zufall: Hyper-LFO";
        case actDelay:  return "Zufall: Delay";
        case actQuadA:  return "Zufall: Quad A";
        case actQuadB:  return "Zufall: Quad B";
        case actPair0:  return "Zufall: Paar 1/2";
        case actPair1:  return "Zufall: Paar 3/4";
        case actPair2:  return "Zufall: Paar 5/6";
        case actPair3:  return "Zufall: Paar 7/8";
        case actReset:  return juce::String::fromUTF8 ("Zur\xc3\xbc" "cksetzen");
        default:        return {};
    }
}

void DroneAchtProcessor::setNorm (juce::RangedAudioParameter* prm, float norm)
{
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (juce::jlimit (0.f, 1.f, norm));
    prm->endChangeGesture();
}

void DroneAchtProcessor::setParam (const juce::String& id, float value)
{
    if (auto* prm = apvts.getParameter (id))
        setNorm (prm, prm->convertTo0to1 (value));
}

void DroneAchtProcessor::performAction (int a)
{
    switch (a)
    {
        case actAll:
            randTune(); for (int p = 0; p < 4; ++p) randPair (p, false);
            randQuad (0); randQuad (1); randLfo(); randDelay();
            break;
        case actMutate: mutate(); break;
        case actChaos:  chaos();  break;
        case actTune:   randTune(); break;
        case actMaster: randMaster(); break;
        case actLfo:    randLfo(); break;
        case actDelay:  randDelay(); break;
        case actQuadA:  randQuad (0); randPair (0, true); randPair (1, true); break;
        case actQuadB:  randQuad (1); randPair (2, true); randPair (3, true); break;
        case actPair0: case actPair1: case actPair2: case actPair3:
            randPair (a - actPair0, true); break;
        case actReset:
            for (auto* p : params) setNorm (p, p->getDefaultValue());
            break;
        default: break;
    }
}

namespace
{
template <typename R> float rnd (R& r, float a, float b) { return a + r.nextFloat() * (b - a); }
template <typename R> float logR (R& r, float a, float b) { return a * std::pow (b / a, r.nextFloat()); }
template <typename R> float pick (R& r, std::initializer_list<float> v) { return *(v.begin() + r.nextInt ((int) v.size())); }
}

void DroneAchtProcessor::randPair (int pr, bool withTune)
{
    if (withTune)
    {
        const float t = pick (rng, { -12, -7, -5, 0, 0, 0, 3, 5, 7 });
        setParam (idx ("tune", pr * 2),     t + rnd (rng, -.05f, .05f));
        setParam (idx ("tune", pr * 2 + 1), juce::jlimit (-12.f, 12.f, t + pick (rng, { 0, 0, 0, 7, 12, -12 }) + rnd (rng, -.15f, .15f)));
    }
    setParam (idx ("mod", pr),   std::pow (rnd (rng, 0, .75f), 1.3f));
    setParam (idx ("sharp", pr), rnd (rng, 0, .85f));
}

void DroneAchtProcessor::randQuad (int q)
{
    setParam (idx ("src", q),   (float) rng.nextInt (3));
    setParam (idx ("pitch", q), pick (rng, { 0, 0, -5, -12, 5, 7 }));
    setParam (idx ("hold", q),  rng.nextFloat() < .5f ? rnd (rng, .3f, .7f) : 0.f);
    for (int i = q * 4; i < q * 4 + 4; ++i)
        setParam (idx ("fast", i), rng.nextFloat() < .55f ? 1.f : 0.f);
}

void DroneAchtProcessor::randTune()
{
    for (int pr = 0; pr < 4; ++pr)
    {
        const float t = pick (rng, { -12, -7, -5, 0, 0, 0, 3, 5, 7 });
        setParam (idx ("tune", pr * 2),     t + rnd (rng, -.05f, .05f));
        setParam (idx ("tune", pr * 2 + 1), juce::jlimit (-12.f, 12.f, t + pick (rng, { 0, 0, 0, 7, 12, -12 }) + rnd (rng, -.15f, .15f)));
    }
    setParam ("pitch0", pick (rng, { 0, 0, -5, -12, 5 }));
    setParam ("pitch1", pick (rng, { 0, 0, -5, -12, 7 }));
}

void DroneAchtProcessor::randMaster()
{
    setParam ("drive",   std::pow (rnd (rng, 0, .8f), 1.4f));
    setParam ("vib",     std::pow (rng.nextFloat(), 2.f) * .6f);
    setParam ("vibRate", logR (rng, .2f, 9.f));
}

void DroneAchtProcessor::randLfo()
{
    setParam ("lfoA", logR (rng, .03f, 3.f));
    setParam ("lfoB", logR (rng, .1f, 14.f));
    setParam ("lfoMode", (float) rng.nextInt (2));
    setParam ("lfoWave", (float) rng.nextInt (2));
}

void DroneAchtProcessor::randDelay()
{
    setParam ("t1",   logR (rng, .06f, 1.2f));
    setParam ("t2",   logR (rng, .06f, 1.2f));
    setParam ("fb",   rnd (rng, .25f, .8f));
    setParam ("dmod", rnd (rng, 0, .7f));
    setParam ("dmix", rnd (rng, .15f, .6f));
}

void DroneAchtProcessor::mutate()
{
    // Kleine Schritte um den aktuellen Klang herum
    for (auto* p : params)
    {
        const auto id = p->getParameterID();
        if (id == "vol") continue;
        if (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr)
        {
            if (rng.nextFloat() < .06f) setNorm (p, p->getValue() > .5f ? 0.f : 1.f);
            continue;
        }
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
        {
            if (rng.nextFloat() < .1f) setParam (id, (float) rng.nextInt (c->choices.size()));
            continue;
        }
        const bool fine = id.startsWith ("tune") || id.startsWith ("pitch");
        const float step = fine ? .006f : .06f;
        const float g = (float) (rng.nextFloat() + rng.nextFloat() + rng.nextFloat() - 1.5) * 2.f; // ~ Gauss
        setNorm (p, p->getValue() + g * step);
    }
}

void DroneAchtProcessor::chaos()
{
    // Alles quer durch den Wertebereich, nur Lautstärke und Feedback bleiben gezähmt
    for (auto* p : params)
    {
        const auto id = p->getParameterID();
        if (id == "vol") continue;
        if (id == "fb")   { setParam (id, rnd (rng, .4f, .9f)); continue; }
        if (id == "dmix") { setParam (id, rnd (rng, .2f, .75f)); continue; }
        if (id == "drive"){ setParam (id, rnd (rng, .2f, .9f)); continue; }
        setNorm (p, rng.nextFloat());
    }
    setParam ("hold0", rnd (rng, .4f, .8f));
}

//==============================================================================
void DroneAchtProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
    {
        xml->deleteAllChildElementsWithTagName ("MIDIMAP");
        auto* map = xml->createNewChildElement ("MIDIMAP");
        for (int c = 0; c < 128; ++c)
        {
            const int t = ccMap[(size_t) c].load();
            if (t < 0) continue;
            auto* e = map->createNewChildElement ("CC");
            e->setAttribute ("n", c);
            e->setAttribute ("t", targetKey (t));
        }
        copyXmlToBinary (*xml, destData);
    }
}

void DroneAchtProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    if (auto* map = xml->getChildByName ("MIDIMAP"))
    {
        for (auto& m : ccMap) m.store (-1);
        for (auto* e : map->getChildWithTagNameIterator ("CC"))
        {
            const int c = e->getIntAttribute ("n", -1);
            const int t = targetFromKey (e->getStringAttribute ("t"));
            if (c >= 0 && c < 128 && t >= 0) ccMap[(size_t) c].store (t);
        }
        bumpMap();
    }
    xml->deleteAllChildElementsWithTagName ("MIDIMAP");
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* DroneAchtProcessor::createEditor() { return new DroneAchtEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DroneAchtProcessor(); }
