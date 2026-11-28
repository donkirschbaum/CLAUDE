// Shruthi XT plugin processor. GPL v3.0.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ShruthiBank.h"

using namespace shruthi;

enum MsgType { mNoteOn, mNoteOff, mJam, mBend, mMod, mAt, mCC, mPanic };

ShruthiProcessor::ShruthiProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SHRUTHI", createLayout())
{
    for (int i = 0; i < kNumParams; ++i)
        paramPtr[(size_t) i] = apvts.getRawParameterValue (kParamInfo[i].id);
    volPtr = apvts.getRawParameterValue ("vol");
    jamPtr = apvts.getRawParameterValue ("jam");
    for (int i = 0; i < 12; ++i)
        for (int j = 0; j < 3; ++j)
            mtxPtr[(size_t) i][(size_t) j] = apvts.getRawParameterValue ("m" + juce::String (i + 1) + "sda"[j]);
    for (int i = 0; i < kNumParams; ++i) mappableIds.add (kParamInfo[i].id);
    for (int i = 0; i < 12; ++i) for (int j = 0; j < 3; ++j) mappableIds.add ("m" + juce::String (i + 1) + "sda"[j]);
    mappableIds.add ("vol"); mappableIds.add ("jam");
    for (auto& id : mappableIds) mappable.push_back (apvts.getParameter (id));
    for (int i = 0; i < 128; ++i) { ccParam[(size_t) i].store (-1); ccMode[(size_t) i].store (0); }
    {
        auto st = loadStore();
        if (st.isObject() && st.hasProperty ("ccmap")) loadMapFromVar (st["ccmap"]);
        else resetMap (true);
    }
    Patch p; loadFactory (0, p, Patch());
    extra = p; setParamsFromPatch (p);
    pushPatchToAudio();
}

juce::AudioProcessorValueTreeState::ParameterLayout ShruthiProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& in = kParamInfo[i];
        ps.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { in.id, 1 }, in.id, in.min, in.max, in.def));
    }
    static const int initMtx[12][3] = {{0,6,0},{1,2,0},{1,3,0},{1,8,0},{2,8,0},{21,2,0},{21,3,0},{20,1,63},{21,1,16},{8,7,32},{0,7,0},{0,0,0}};
    for (int i = 0; i < 12; ++i)
    {
        const juce::String n (i + 1);
        ps.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "m" + n + "s", 1 }, "mod " + n + " source", 0, 31, initMtx[i][0]));
        ps.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "m" + n + "d", 1 }, "mod " + n + " destination", 0, 26, initMtx[i][1]));
        ps.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "m" + n + "a", 1 }, "mod " + n + " amount", -63, 63, initMtx[i][2]));
    }
    ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "vol", 1 }, "volume", 0.0f, 1.0f, 0.75f));
    ps.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "jam", 1 }, "jam", false));
    return { ps.begin(), ps.end() };
}

bool ShruthiProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& o = layouts.getMainOutputChannelSet();
    return o == juce::AudioChannelSet::stereo() || o == juce::AudioChannelSet::mono();
}

void ShruthiProcessor::prepareToPlay (double sampleRate, int)
{
    if (! engine) engine = std::make_unique<Engine> (sampleRate);
    else engine->setSampleRate (sampleRate);
    const juce::SpinLock::ScopedLockType sl (lock);
    pending = true;
}

void ShruthiProcessor::pushMsg (Msg m)
{
    const auto scope = fifo.write (1);
    if (scope.blockSize1 > 0) msgs[(size_t) scope.startIndex1] = m;
    else if (scope.blockSize2 > 0) msgs[(size_t) scope.startIndex2] = m;
}

void ShruthiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (! engine) { buffer.clear(); return; }

    {
        const juce::SpinLock::ScopedTryLockType tl (lock);
        if (tl.isLocked() && pending) { audioPatch = pendingPatch; engine->setSys (pendingSys); pending = false; }
    }
    for (int i = 0; i < kNumParams; ++i) audioPatch.v[i] = (int) std::lround (paramPtr[(size_t) i]->load());
    for (int i = 0; i < 12; ++i) for (int j = 0; j < 3; ++j) audioPatch.mtx[i][j] = (int) std::lround (mtxPtr[(size_t) i][(size_t) j]->load());
    engine->setPatch (audioPatch);
    engine->setVolume (volPtr->load());

    double bpm = 0;
    if (sync)
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm()) bpm = *b;
    engine->setHostTempo (bpm);

    const bool jamNow = jamPtr->load() > 0.5f;
    if (jamNow != lastJam) { engine->setJam (jamNow); lastJam = jamNow; }

    {
        const auto scope = fifo.read (fifo.getNumReady());
        auto handle = [this] (const Msg& m) {
            switch (m.type) {
                case mNoteOn: engine->noteOn (m.a, (int) m.f); break;
                case mNoteOff: engine->noteOff (m.a); break;
                case mBend: engine->setBend (m.f); break;
                case mMod: engine->setModWheel (m.f); break;
                case mAt: engine->setAftertouch (m.f); break;
                case mCC: engine->setCC (m.a, m.f); break;
                case mPanic: engine->panic(); break;
                default: break;
            }
        };
        for (int i = 0; i < scope.blockSize1; ++i) handle (msgs[(size_t) (scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i) handle (msgs[(size_t) (scope.startIndex2 + i)]);
    }

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    int pos = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int t = juce::jlimit (0, n, meta.samplePosition);
        if (t > pos) { engine->process (L + pos, R ? R + pos : nullptr, t - pos); pos = t; }
        if (chn > 0 && msg.getChannel() != chn) continue;
        if (msg.isNoteOn()) engine->noteOn (msg.getNoteNumber(), msg.getVelocity());
        else if (msg.isNoteOff()) engine->noteOff (msg.getNoteNumber());
        else if (msg.isPitchWheel()) engine->setBend ((msg.getPitchWheelValue() - 8192) / 8192.0f);
        else if (msg.isChannelPressure()) engine->setAftertouch (msg.getChannelPressureValue() / 127.0f);
        else if (msg.isAftertouch()) engine->setAftertouch (msg.getAfterTouchValue() / 127.0f);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine->panic();
        else if (msg.isController())
        {
            const int c = msg.getControllerNumber(); const float v = msg.getControllerValue() / 127.0f;
            const int lt = learnTarget.load();
            if (lt >= 0 && c != 0 && c != 32 && c < 120)
            {
                for (int k = 0; k < 128; ++k) if (ccParam[(size_t) k].load() == lt) ccParam[(size_t) k].store (-1);
                ccParam[(size_t) c].store (lt);
                const int val = msg.getControllerValue();
                ccMode[(size_t) c].store (lt == (int) mappableIds.size() - 1 ? 3 : ((val == 1 || val == 127 || val == 63 || val == 65) ? 1 : 0));
                learnTarget.store (-1); ++mapVersion;
                continue;
            }
            if (ccParam[(size_t) c].load() >= 0) { applyCC (c, msg.getControllerValue()); continue; }
            if (c == 1) engine->setModWheel (v);
            else if (c == 2) engine->setCC (2, v);
            else if (c == 4) engine->setCC (3, v);
            else if (c == 16 || c == 17) engine->setCC (c - 16, v);
        }
    }
    if (pos < n) engine->process (L + pos, R ? R + pos : nullptr, n - pos);
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
    lastStep.store (engine->currentStep());
    midi.clear();
}

/* ---------- patch <-> params ---------- */
void ShruthiProcessor::setParamsFromPatch (const Patch& p)
{
    for (int i = 0; i < kNumParams; ++i)
        if (auto* prm = apvts.getParameter (kParamInfo[i].id))
        {
            const float norm = prm->convertTo0to1 ((float) p.v[i]);
            if (std::abs (prm->getValue() - norm) > 1e-6f) { prm->beginChangeGesture(); prm->setValueNotifyingHost (norm); prm->endChangeGesture(); }
        }
    for (int i = 0; i < 12; ++i)
        for (int j = 0; j < 3; ++j)
            if (auto* prm = apvts.getParameter ("m" + juce::String (i + 1) + "sda"[j]))
            {
                const float norm = prm->convertTo0to1 ((float) p.mtx[i][j]);
                if (std::abs (prm->getValue() - norm) > 1e-6f) { prm->beginChangeGesture(); prm->setValueNotifyingHost (norm); prm->endChangeGesture(); }
            }
}

Patch ShruthiProcessor::patchFromParams() const
{
    Patch p = extra;
    for (int i = 0; i < kNumParams; ++i) p.v[i] = (int) std::lround (paramPtr[(size_t) i]->load());
    for (int i = 0; i < 12; ++i) for (int j = 0; j < 3; ++j) p.mtx[i][j] = (int) std::lround (mtxPtr[(size_t) i][(size_t) j]->load());
    return p;
}

void ShruthiProcessor::readParamValues (int* out) const
{
    for (int i = 0; i < kNumParams; ++i) out[i] = (int) std::lround (paramPtr[(size_t) i]->load());
}

void ShruthiProcessor::readMtxValues (int* out) const
{
    for (int i = 0; i < 12; ++i) for (int j = 0; j < 3; ++j) out[i * 3 + j] = (int) std::lround (mtxPtr[(size_t) i][(size_t) j]->load());
}

bool ShruthiProcessor::jamState() const { return jamPtr->load() > 0.5f; }

void ShruthiProcessor::pushPatchToAudio()
{
    const juce::SpinLock::ScopedLockType sl (lock);
    pendingPatch = patchFromParams(); pendingSys = sys; pending = true;
}

static juce::var patchToVar (const Patch& p)
{
    auto* o = new juce::DynamicObject();
    for (int i = 0; i < kNumParams; ++i) o->setProperty (kParamInfo[i].id, p.v[i]);
    juce::Array<juce::var> mtx;
    for (int i = 0; i < 12; ++i) { juce::Array<juce::var> r; for (int j = 0; j < 3; ++j) r.add (p.mtx[i][j]); mtx.add (r); }
    o->setProperty ("mtx", mtx);
    juce::Array<juce::var> st;
    for (int i = 0; i < 16; ++i) { auto* s = new juce::DynamicObject(); s->setProperty ("g", p.steps[i].g); s->setProperty ("n", p.steps[i].n); s->setProperty ("v", p.steps[i].v); s->setProperty ("c", p.steps[i].c); st.add (juce::var (s)); }
    o->setProperty ("steps", st);
    o->setProperty ("name", juce::String (p.name));
    return juce::var (o);
}

static void varToPatch (const juce::var& v, Patch& p)
{
    if (! v.isObject()) return;
    for (int i = 0; i < kNumParams; ++i)
        if (v.hasProperty (kParamInfo[i].id))
            p.v[i] = juce::jlimit (kParamInfo[i].min, kParamInfo[i].max, (int) v[kParamInfo[i].id]);
    if (auto* m = v["mtx"].getArray())
        for (int i = 0; i < juce::jmin (12, m->size()); ++i)
            if (auto* r = (*m)[i].getArray())
                for (int j = 0; j < juce::jmin (3, r->size()); ++j) p.mtx[i][j] = (int) (*r)[j];
    if (auto* s = v["steps"].getArray())
        for (int i = 0; i < juce::jmin (16, s->size()); ++i)
        {
            const auto& e = (*s)[i];
            p.steps[i].g = juce::jlimit (0, 2, (int) e["g"]); p.steps[i].n = juce::jlimit (0, 127, (int) e["n"]);
            p.steps[i].v = juce::jlimit (0, 7, (int) e["v"]); p.steps[i].c = juce::jlimit (0, 15, (int) e["c"]);
        }
    if (v.hasProperty ("name")) p.name = v["name"].toString().toStdString();
}

juce::var ShruthiProcessor::stateAsVar()
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("p", patchToVar (patchFromParams()));
    auto* g = new juce::DynamicObject();
    g->setProperty ("brd", sys.brd); g->setProperty ("oct", sys.oct); g->setProperty ("raga", sys.raga);
    g->setProperty ("tune", sys.tune); g->setProperty ("lofi", sys.lofi); g->setProperty ("chn", chn);
    g->setProperty ("lcdc", lcdc); g->setProperty ("sync", sync); g->setProperty ("vol", volPtr->load());
    o->setProperty ("g", juce::var (g));
    o->setProperty ("cur", currentProgram);
    o->setProperty ("jam", jamState());
    o->setProperty ("store", loadStore());
    return juce::var (o);
}

void ShruthiProcessor::applyPatchFromUI (const juce::var& v)
{
    Patch p = patchFromParams();
    varToPatch (v, p);
    extra = p;
    setParamsFromPatch (p);
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        pendingPatch = p; pendingSys = sys; pending = true;
    }
    if (v.hasProperty ("cur")) currentProgram = (int) v["cur"];
}

void ShruthiProcessor::applySysFromUI (const juce::var& g)
{
    if (! g.isObject()) return;
    auto get = [&g] (const char* k, int d) { return g.hasProperty (k) ? (int) g[k] : d; };
    sys.brd = juce::jlimit (0, 7, get ("brd", sys.brd)); sys.oct = juce::jlimit (-2, 2, get ("oct", sys.oct));
    sys.raga = juce::jlimit (0, 32, get ("raga", sys.raga)); sys.tune = juce::jlimit (-127, 127, get ("tune", sys.tune));
    sys.lofi = get ("lofi", sys.lofi) ? 1 : 0; chn = juce::jlimit (0, 16, get ("chn", chn));
    lcdc = get ("lcdc", lcdc); sync = get ("sync", sync) ? 1 : 0;
    pushPatchToAudio();
}

void ShruthiProcessor::postUIEvent (const juce::var& m)
{
    const juce::String t = m["t"].toString();
    if (t == "on") pushMsg ({ mNoteOn, (int) m["n"], (float) (int) m["v"] });
    else if (t == "off") pushMsg ({ mNoteOff, (int) m["n"], 0 });
    else if (t == "bend") pushMsg ({ mBend, 0, (float) m["v"] });
    else if (t == "mw") pushMsg ({ mMod, 0, (float) m["v"] });
    else if (t == "at") pushMsg ({ mAt, 0, (float) m["v"] });
    else if (t == "cc") pushMsg ({ mCC, (int) m["i"], (float) m["v"] });
    else if (t == "panic") { pushMsg ({ mPanic, 0, 0 }); if (auto* p = apvts.getParameter ("jam")) p->setValueNotifyingHost (0.0f); }
    else if (t == "jam") { if (auto* p = apvts.getParameter ("jam")) { p->beginChangeGesture(); p->setValueNotifyingHost ((bool) m["on"] ? 1.0f : 0.0f); p->endChangeGesture(); } }
    else if (t == "vol") { if (auto* p = apvts.getParameter ("vol")) { p->beginChangeGesture(); p->setValueNotifyingHost ((float) m["v"]); p->endChangeGesture(); } }
    else if (t == "cur") currentProgram = (int) m["i"];
}

/* ---------- programs ---------- */
int ShruthiProcessor::getNumPrograms() { return numFactoryPrograms(); }
const juce::String ShruthiProcessor::getProgramName (int i) { return juce::String (i + 1).paddedLeft ('0', 3) + " " + factoryName (i); }
void ShruthiProcessor::setCurrentProgram (int i)
{
    if (i < 0 || i >= numFactoryPrograms()) return;
    Patch p; if (! loadFactory (i, p, patchFromParams())) return;
    currentProgram = i; extra = p;
    if (juce::MessageManager::getInstanceWithoutCreating() && juce::MessageManager::getInstance()->isThisTheMessageThread())
        setParamsFromPatch (p);
    else
        juce::MessageManager::callAsync ([this, p] { setParamsFromPatch (p); pushPatchToAudio(); ++stateVersion; });
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        pendingPatch = p; pendingSys = sys; pending = true;
    }
    ++stateVersion;
}

/* ---------- state ---------- */
void ShruthiProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    auto xml = st.createXml();
    auto* ex = xml->createNewChildElement ("SHRUTHI_EXTRA");
    auto* o = new juce::DynamicObject();
    o->setProperty ("p", patchToVar (patchFromParams()));
    o->setProperty ("brd", sys.brd); o->setProperty ("oct", sys.oct); o->setProperty ("raga", sys.raga);
    o->setProperty ("tune", sys.tune); o->setProperty ("lofi", sys.lofi); o->setProperty ("chn", chn);
    o->setProperty ("lcdc", lcdc); o->setProperty ("sync", sync); o->setProperty ("cur", currentProgram);
    o->setProperty ("ccmap", mapAsVar());
    ex->setAttribute ("json", juce::JSON::toString (juce::var (o), true));
    copyXmlToBinary (*xml, dest);
}

void ShruthiProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (! xml) return;
    juce::var j;
    if (auto* ex = xml->getChildByName ("SHRUTHI_EXTRA")) { j = juce::JSON::parse (ex->getStringAttribute ("json")); xml->removeChildElement (ex, true); }
    if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
    if (j.isObject())
    {
        Patch p = patchFromParams(); varToPatch (j["p"], p); extra = p;
        sys.brd = (int) j["brd"]; sys.oct = (int) j["oct"]; sys.raga = (int) j["raga"]; sys.tune = (int) j["tune"];
        sys.lofi = (int) j["lofi"]; chn = (int) j["chn"]; lcdc = (int) j["lcdc"]; sync = j.hasProperty ("sync") ? (int) j["sync"] : 1;
        currentProgram = (int) j["cur"];
        if (j.hasProperty ("ccmap")) loadMapFromVar (j["ccmap"]);
    }
    pushPatchToAudio();
    ++stateVersion;
}

/* ---------- user store (imports / user slots), shared by all instances ---------- */
juce::File ShruthiProcessor::storeFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Shruthi XT").getChildFile ("store.json");
}
juce::var ShruthiProcessor::loadStore()
{
    auto f = storeFile();
    return f.existsAsFile() ? juce::JSON::parse (f) : juce::var();
}
void ShruthiProcessor::saveStore (const juce::var& data)
{
    auto f = storeFile(); f.getParentDirectory().createDirectory();
    auto cur = loadStore();
    auto* o = cur.isObject() ? cur.getDynamicObject() : new juce::DynamicObject();
    juce::var keep (o);
    if (auto* d = data.getDynamicObject())
        for (auto& p : d->getProperties()) o->setProperty (p.name, p.value);
    f.replaceWithText (juce::JSON::toString (keep, true));
}

/* ---------- MIDI CC mapping ---------- */
void ShruthiProcessor::applyCC (int cc, int value)
{
    const int idx = ccParam[(size_t) cc].load();
    if (idx < 0 || idx >= (int) mappable.size()) return;
    auto* p = mappable[(size_t) idx]; if (! p) return;
    const int mode = ccMode[(size_t) cc].load();
    const auto& r = p->getNormalisableRange();
    const bool isFloat = dynamic_cast<juce::AudioParameterFloat*> (p) != nullptr;
    if (mode == 3) { if (value > 0) p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f); return; }
    if (mode == 0)
    {
        if (isFloat) { p->setValueNotifyingHost (value / 127.0f); return; }
        const int span = (int) (r.end - r.start) + 1;
        const float v = (float) r.start + (float) ((value * span) >> 7);
        p->setValueNotifyingHost (p->convertTo0to1 (juce::jmin ((float) r.end, v)));
        return;
    }
    int delta = mode == 1 ? value - 64 : (value < 64 ? value : value - 128);
    if (isFloat) { p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->getValue() + delta / 127.0f)); return; }
    const float cur = std::round (p->convertFrom0to1 (p->getValue()));
    p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit ((float) r.start, (float) r.end, cur + (float) delta)));
}

juce::var ShruthiProcessor::mapAsVar() const
{
    juce::Array<juce::var> list;
    for (int c = 0; c < 128; ++c)
    {
        const int idx = ccParam[(size_t) c].load();
        if (idx >= 0 && idx < mappableIds.size()) { juce::Array<juce::var> e; e.add (c); e.add (mappableIds[idx]); e.add (ccMode[(size_t) c].load()); list.add (e); }
    }
    return list;
}

void ShruthiProcessor::loadMapFromVar (const juce::var& v)
{
    if (! v.isArray()) return;
    for (int i = 0; i < 128; ++i) { ccParam[(size_t) i].store (-1); ccMode[(size_t) i].store (0); }
    for (auto& e : *v.getArray())
        if (auto* a = e.getArray(); a && a->size() >= 3)
        {
            const int c = (int) (*a)[0], idx = mappableIds.indexOf ((*a)[1].toString());
            if (c >= 0 && c < 128 && idx >= 0) { ccParam[(size_t) c].store (idx); ccMode[(size_t) c].store (juce::jlimit (0, 3, (int) (*a)[2])); }
        }
    ++mapVersion;
}

void ShruthiProcessor::saveMapGlobal()
{
    auto* o = new juce::DynamicObject(); o->setProperty ("ccmap", mapAsVar());
    saveStore (juce::var (o));
}

void ShruthiProcessor::setLearn (const juce::String& id)
{
    learnTarget.store (id.isEmpty() ? -1 : mappableIds.indexOf (id));
    ++mapVersion;
}

void ShruthiProcessor::unmap (int cc)
{
    if (cc >= 0 && cc < 128) ccParam[(size_t) cc].store (-1);
    ++mapVersion; saveMapGlobal();
}

void ShruthiProcessor::setMapMode (int cc, int mode)
{
    if (cc >= 0 && cc < 128) ccMode[(size_t) cc].store (juce::jlimit (0, 3, mode));
    ++mapVersion; saveMapGlobal();
}

void ShruthiProcessor::resetMap (bool shruthiDefaults)
{
    for (int i = 0; i < 128; ++i) { ccParam[(size_t) i].store (-1); ccMode[(size_t) i].store (0); }
    if (shruthiDefaults)
    {
        // CC assignments of the Shruthi-1 firmware (parameter.cc)
        static const std::pair<int, const char*> def[] = {
            {20,"o1s"},{21,"o1p"},{22,"o1r"},{23,"sub"},{24,"o2s"},{25,"o2p"},{26,"o2r"},{27,"o2d"},
            {28,"op"},{29,"bal"},{30,"subl"},{31,"nse"},{14,"cut"},{74,"cut"},{15,"res"},{71,"res"},
            {102,"fenv"},{103,"flfo"},{104,"e1a"},{105,"e1d"},{106,"e1s"},{107,"e1r"},
            {108,"e2a"},{73,"e2a"},{109,"e2d"},{110,"e2s"},{111,"e2r"},{72,"e2r"},
            {112,"l1w"},{113,"l1r"},{114,"l1a"},{115,"l1m"},{116,"l2w"},{117,"l2r"},{118,"l2a"},{119,"l2m"},
            {75,"mode"},{76,"grv"},{77,"gam"},{78,"adir"},{79,"arng"},{80,"apat"},{81,"div"},
            {84,"port"},{5,"port"},{68,"lgt"},{12,"cut2"},{13,"res2"},{85,"fm1"},{86,"fm2"} };
        for (auto& d : def) { const int idx = mappableIds.indexOf (d.second); if (idx >= 0) ccParam[(size_t) d.first].store (idx); }
    }
    ++mapVersion;
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr) saveMapGlobal();
}

juce::AudioProcessorEditor* ShruthiProcessor::createEditor() { return new ShruthiEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ShruthiProcessor(); }
