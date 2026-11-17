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
            if (std::abs (prm->getValue() - norm) > 1e-6f) prm->setValueNotifyingHost (norm);
        }
}

Patch ShruthiProcessor::patchFromParams() const
{
    Patch p = extra;
    for (int i = 0; i < kNumParams; ++i) p.v[i] = (int) std::lround (paramPtr[(size_t) i]->load());
    return p;
}

void ShruthiProcessor::readParamValues (int* out) const
{
    for (int i = 0; i < kNumParams; ++i) out[i] = (int) std::lround (paramPtr[(size_t) i]->load());
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
    else if (t == "jam") { if (auto* p = apvts.getParameter ("jam")) p->setValueNotifyingHost ((bool) m["on"] ? 1.0f : 0.0f); }
    else if (t == "vol") { if (auto* p = apvts.getParameter ("vol")) p->setValueNotifyingHost ((float) m["v"]); }
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
    f.replaceWithText (juce::JSON::toString (data, true));
}

juce::AudioProcessorEditor* ShruthiProcessor::createEditor() { return new ShruthiEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ShruthiProcessor(); }
