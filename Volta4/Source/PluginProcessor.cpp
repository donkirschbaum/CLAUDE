// VOLTA·4 – Plugin-Prozessor.
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace volta;

static const char* kDefaultCables =
    "[[\"sA\",\"kick.trig\",\"#ff7a1a\"],[\"sB\",\"snare.trig\",\"#ffd23f\"],[\"c2\",\"hats.trig\",\"#3fc5ff\"],"
    "[\"sC\",\"bass.trig\",\"#ff4f6d\"],[\"rnd\",\"bass.pitch\",\"#b6f05a\"],[\"l1\",\"bass.tone\",\"#c08bff\"],"
    "[\"l2\",\"hats.dec\",\"#5af0c8\"],[\"sD\",\"probI\",\"#f5f5f5\"],[\"probO\",\"snare.trig\",\"#ff7a1a\"]]";

VoltaProcessor::VoltaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VOLTA4", createLayout())
{
    const auto& list = paramList();
    for (size_t i = 0; i < list.size(); ++i)
    {
        const juce::String id (list[i].id);
        params.push_back (apvts.getParameter (id));
        raw.push_back (apvts.getRawParameterValue (id));
        types.push_back (list[i].type);
        idIndex.set (id, (int) i);
    }
    iBpm = indexOf ("bpm"); iPlay = indexOf ("play"); iHost = indexOf ("host");
    resetMap();
    cablesVar = juce::JSON::parse (kDefaultCables);
    parseCables();
    events.reserve (1024);
}

juce::AudioProcessorValueTreeState::ParameterLayout VoltaProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    for (const auto& p : paramList())
    {
        const juce::ParameterID pid { p.id, 1 };
        switch (p.type)
        {
            case PT::Float:
                ps.push_back (std::make_unique<juce::AudioParameterFloat> (pid, p.name, juce::NormalisableRange<float> (0.f, 1.f), p.def,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.f)) + " %"; })));
                break;
            case PT::Choice:
                ps.push_back (std::make_unique<juce::AudioParameterChoice> (pid, p.name, juce::StringArray { "Sinus", "Dreieck", "Saege", "Rechteck" }, (int) p.def));
                break;
            case PT::Bool:
                ps.push_back (std::make_unique<juce::AudioParameterBool> (pid, p.name, p.def > .5f));
                break;
            case PT::Int:
                ps.push_back (std::make_unique<juce::AudioParameterInt> (pid, p.name, (int) p.min, (int) p.max, (int) p.def));
                break;
        }
    }
    return { ps.begin(), ps.end() };
}

bool VoltaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& o = layouts.getMainOutputChannelSet();
    return o == juce::AudioChannelSet::stereo() || o == juce::AudioChannelSet::mono();
}

void VoltaProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    engine.prepare (sampleRate);
    wasPlaying = false;
}

void VoltaProcessor::readParams (Params& p) const
{
    // Reihenfolge wie in Params.h: bpm swing vol dtime dfb dmix l1r l2r prob div l1s l2s
    auto f = [this] (int i) { return raw[(size_t) i]->load(); };
    p.bpm = f (0); p.swing = f (1); p.vol = f (2); p.dtime = f (3); p.dfb = f (4); p.dmix = f (5);
    p.l1r = f (6); p.l2r = f (7); p.prob = f (8); p.div = f (9);
    p.l1s = (int) std::lround (f (10)); p.l2s = (int) std::lround (f (11));
    // Kanal-Parameter liegen in der Liste ab Index 12, je 7 Werte
    for (int c = 0; c < kNumCh; ++c)
    {
        const int b = 12 + c * 7;
        auto& q = p.ch[c];
        q.tune = raw[(size_t) b]->load(); q.decay = raw[(size_t) b + 1]->load(); q.tone = raw[(size_t) b + 2]->load();
        q.snap = raw[(size_t) b + 3]->load(); q.mod = raw[(size_t) b + 4]->load(); q.level = raw[(size_t) b + 5]->load(); q.fx = raw[(size_t) b + 6]->load();
    }
    const int sb = 12 + kNumCh * 7;   // je Reihe: len, mute, solo
    for (int r = 0; r < 4; ++r)
    {
        p.len[r] = (int) std::lround (raw[(size_t) (sb + r * 3)]->load());
        p.mute[r] = raw[(size_t) (sb + r * 3 + 1)]->load() > .5f;
        p.solo[r] = raw[(size_t) (sb + r * 3 + 2)]->load() > .5f;
    }
    const int stb = sb + 12;
    for (int r = 0; r < 4; ++r)
        for (int k = 0; k < 16; ++k) p.steps[r][k] = raw[(size_t) (stb + r * 16 + k)]->load() > .5f;
}

void VoltaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);

    {
        const juce::SpinLock::ScopedTryLockType tl (cableLock);
        if (tl.isLocked() && cablesPending) { engine.setCables (pendingCables); cablesPending = false; }
    }
    Params P; readParams (P); engine.setParams (P);

    {
        const auto sc = fifo.read (fifo.getNumReady());
        for (int i = 0; i < sc.blockSize1; ++i) engine.trigger (pads[(size_t) (sc.startIndex1 + i)], 0, 0, 0);
        for (int i = 0; i < sc.blockSize2; ++i) engine.trigger (pads[(size_t) (sc.startIndex2 + i)], 0, 0, 0);
    }

    // Transport: interne Clock oder Host (DAW)
    const bool hostMode = raw[(size_t) iHost]->load() > .5f;
    double bpm = engine.bpmFromParam(), ppq0 = 0;
    bool playing = false;
    if (hostMode)
    {
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                if (auto b = pos->getBpm()) bpm = *b;
                if (auto q = pos->getPpqPosition()) { ppq0 = *q; playing = pos->getIsPlaying(); }
            }
        hostBpm.store (bpm);
    }
    else
    {
        playing = raw[(size_t) iPlay]->load() > .5f;
        if (playing && ! wasPlaying) internalPpq = 0;
        ppq0 = internalPpq;
    }
    bpm = juce::jlimit (20.0, 400.0, bpm);
    const double pps = bpm / 60.0 / sr;
    engine.setStepDuration (60.0 / bpm / 4.0);
    transportPlaying.store (playing);

    events.clear();
    if (playing)
    {
        const double ppqEnd = ppq0 + n * pps;
        if (! wasPlaying) engine.resetSequencer();
        if (! wasPlaying || std::abs (ppq0 - lastPpqEnd) > 1e-3)
            nextStep = (int64_t) std::ceil (ppq0 * 4.0 - 0.04);
        const double sw = P.swing * 0.5;
        for (int guard = 0; guard < 64; ++guard)
        {
            const double ft = ((double) nextStep + ((nextStep & 1) ? sw : 0.0)) / 4.0;
            if (ft >= ppqEnd) break;
            if (nextStep >= 0)
            {
                const int off = juce::jlimit (0, n - 1, (int) std::floor ((ft - ppq0) / pps));
                if (events.size() < events.capacity()) events.push_back ({ off, 0, nextStep, { 0, 0, 0 }, 0 });
            }
            ++nextStep;
        }
        lastPpqEnd = ppqEnd;
        if (! hostMode) internalPpq = ppqEnd;
    }
    wasPlaying = playing;

    for (const auto meta : midi)
    {
        if (events.size() >= events.capacity()) break;
        const int len = meta.numBytes;
        if (len < 1 || len > 3) continue;
        Ev e { juce::jlimit (0, juce::jmax (0, n - 1), meta.samplePosition), 1, 0, { 0, 0, 0 }, len };
        for (int i = 0; i < len; ++i) e.d[i] = meta.data[i];
        events.push_back (e);
    }
    for (size_t i = 1; i < events.size(); ++i)   // Einfügesortierung, stabil und ohne Speicheranforderung
        for (size_t j = i; j > 0 && events[j - 1].off > events[j].off; --j) std::swap (events[j - 1], events[j]);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    int pos = 0;
    for (const auto& e : events)
    {
        if (e.off > pos) { engine.render (L + pos, R ? R + pos : nullptr, e.off - pos); pos = e.off; }
        if (e.type == 0) engine.doStep (e.step);
        else handleMidi (e.d, e.len);
    }
    if (pos < n) engine.render (L + pos, R ? R + pos : nullptr, n - pos);
    engine.publishCV();
    midi.clear();
}

/* ---------- MIDI ---------- */
void VoltaProcessor::handleMidi (const uint8_t* d, int len)
{
    const int status = d[0] & 0xF0;
    if (status == 0x90 && len == 3 && d[2] > 0)
    {
        const int note = d[1]; const float vel = d[2] / 127.f;
        if (note == 35 || note == 36) engine.trigger (0, 0, 0, 0, vel);
        else if (note >= 37 && note <= 40) engine.trigger (1, 0, 0, 0, vel);
        else if (note == 42 || note == 44 || note == 46) engine.trigger (3, 0, note == 46 ? 1.f : 0.f, 0, vel);
        else if (note >= 48) engine.trigger (2, 0, 0, 0, vel, note);
        return;
    }
    if (status == 0xB0 && len == 3)
    {
        const int cc = d[1], val = d[2];
        const int lt = learnTarget.load();
        if (lt >= 0 && cc != 0 && cc != 32 && cc < 120)
        {
            for (auto& c : ccParam) if (c.load() == lt) c.store (-1);
            ccParam[(size_t) cc].store (lt);
            learnTarget.store (-1); lastLearnedCC.store (cc); ++mapVersion;
            return;
        }
        if (ccParam[(size_t) cc].load() >= 0) applyCC (cc, val);
    }
}

void VoltaProcessor::applyCC (int cc, int value)
{
    const int idx = ccParam[(size_t) cc].load();
    if (idx < 0 || idx >= (int) params.size()) return;
    auto* p = params[(size_t) idx];
    switch (types[(size_t) idx])
    {
        case PT::Float: p->setValueNotifyingHost (value / 127.f); break;
        case PT::Bool:  p->setValueNotifyingHost (value >= 64 ? 1.f : 0.f); break;
        default:
        {
            const auto& r = p->getNormalisableRange();
            const float v = std::round (r.start + (r.end - r.start) * (value / 127.f));
            p->setValueNotifyingHost (p->convertTo0to1 (v));
        }
    }
}

void VoltaProcessor::resetMap()
{
    for (auto& c : ccParam) c.store (-1);
    const auto& list = paramList();
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].defaultCC >= 0 && list[i].defaultCC < 128) ccParam[(size_t) list[i].defaultCC].store ((int) i);
    ++mapVersion;
}

void VoltaProcessor::unlearn (const juce::String& id)
{
    const int i = indexOf (id);
    for (auto& c : ccParam) if (c.load() == i) c.store (-1);
    ++mapVersion;
}

juce::var VoltaProcessor::mapAsVar() const
{
    auto* o = new juce::DynamicObject();
    for (int cc = 0; cc < 128; ++cc)
    {
        const int i = ccParam[(size_t) cc].load();
        if (i >= 0 && i < (int) params.size()) o->setProperty (juce::Identifier (paramList()[(size_t) i].id), cc);
    }
    return juce::var (o);
}

/* ---------- Oberfläche ---------- */
void VoltaProcessor::setFromUI (int i, float v)
{
    if (i < 0 || i >= (int) params.size()) return;
    auto* p = params[(size_t) i];
    const float norm = p->convertTo0to1 (v);
    if (std::abs (p->getValue() - norm) > 1e-7f) p->setValueNotifyingHost (norm);
}

void VoltaProcessor::setCablesFromUI (const juce::var& list)
{
    cablesVar = list;
    parseCables();
}

void VoltaProcessor::parseCables()
{
    std::vector<Cable> c;
    if (auto* arr = cablesVar.getArray())
        for (const auto& e : *arr)
        {
            if (! e.isArray() || e.size() < 2) continue;
            const int f = srcFromId (e[0].toString().toStdString()), t = inFromId (e[1].toString().toStdString());
            if (f >= 0 && t >= 0) c.push_back ({ f, t });
        }
    const juce::SpinLock::ScopedLockType sl (cableLock);
    pendingCables = std::move (c); cablesPending = true;
}

/* ---------- Zustand ---------- */
void VoltaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto st = apvts.copyState();
    st.setProperty ("cables", juce::JSON::toString (cablesVar, true), nullptr);
    st.setProperty ("pi", presetIndex, nullptr);
    st.setProperty ("ccmap", juce::JSON::toString (mapAsVar(), true), nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, destData);
}

void VoltaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (! xml || ! xml->hasTagName (apvts.state.getType())) return;
    auto st = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (st);
    if (st.hasProperty ("cables")) { cablesVar = juce::JSON::parse (st["cables"].toString()); parseCables(); }
    presetIndex = (int) st.getProperty ("pi", -1);
    if (st.hasProperty ("ccmap"))
    {
        for (auto& c : ccParam) c.store (-1);
        const auto mapVar = juce::JSON::parse (st["ccmap"].toString());
        if (auto* o = mapVar.getDynamicObject())
            for (const auto& p : o->getProperties())
            {
                const int i = indexOf (p.name.toString()); const int cc = (int) p.value;
                if (i >= 0 && cc >= 0 && cc < 128) ccParam[(size_t) cc].store (i);
            }
        ++mapVersion;
    }
    ++stateVersion;
}

juce::AudioProcessorEditor* VoltaProcessor::createEditor() { return new VoltaEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VoltaProcessor(); }
