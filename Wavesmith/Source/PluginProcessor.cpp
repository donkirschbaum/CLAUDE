#include "PluginProcessor.h"
#include "PluginEditor.h"

WavesmithProcessor::WavesmithProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WAVESMITH", ws::createParameterLayout()),
      presets (apvts)
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            paramCache[rp->paramID] = apvts.getRawParameterValue (rp->paramID);

    ws::WavetableBank::get(); // build tables up-front, not on the audio thread

    if (presets.getNumPresets() > 0)
        presets.loadPreset (0);
}

bool WavesmithProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void WavesmithProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    effects.prepare (sampleRate, samplesPerBlock);
}

float WavesmithProcessor::get (const char* id) const { return get (juce::String (id)); }
float WavesmithProcessor::get (const juce::String& id) const
{
    auto it = paramCache.find (id);
    jassert (it != paramCache.end());
    return it != paramCache.end() ? it->second->load (std::memory_order_relaxed) : 0.0f;
}

void WavesmithProcessor::updateSettings()
{
    auto& s = settings;
    for (int o = 0; o < 2; ++o)
    {
        const juce::String p = o == 0 ? "oscA" : "oscB";
        auto& os = s.osc[o];
        os.on = get (p + "On") > 0.5f;
        os.table = (int) get (p + "Table");
        os.pos = get (p + "Pos");
        os.oct = (int) std::lround (get (p + "Oct"));
        os.semi = (int) std::lround (get (p + "Semi"));
        os.fine = get (p + "Fine");
        os.uni = (int) std::lround (get (p + "Uni"));
        os.detune = get (p + "Detune");
        os.spread = get (p + "Spread");
        os.level = get (p + "Level");
        os.pan = get (p + "Pan");
        os.phase = get (p + "Phase");
        os.randPhase = get (p + "Rand") > 0.5f;
    }
    s.fm = get ("fmAmt");
    s.subLevel = get ("subLevel");
    s.subOct = (int) get ("subOct");
    s.subShape = (int) get ("subShape");
    s.noiseLevel = get ("noiseLevel");
    s.noiseColor = get ("noiseColor");

    s.fltOn = get ("fltOn") > 0.5f;
    s.fltType = (int) get ("fltType");
    s.cutoff = get ("fltCutoff");
    s.reso = get ("fltReso");
    s.drive = get ("fltDrive");
    s.fltEnv = get ("fltEnvAmt");
    s.fltKey = get ("fltKey");
    s.fltVel = get ("fltVel");

    s.ampA = get ("ampA"); s.ampD = get ("ampD"); s.ampS = get ("ampS"); s.ampR = get ("ampR");
    s.envA = get ("envA"); s.envD = get ("envD"); s.envS = get ("envS"); s.envR = get ("envR");
    s.ampVel = get ("ampVel");

    // Tempo
    clock.bpm = 120.0;
    clock.hostPlaying = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) clock.bpm = juce::jlimit (20.0, 999.0, *bpm);
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition())
                {
                    clock.hostPlaying = true;
                    clock.hostPpq = *ppq;
                }
        }
    }

    for (int l = 0; l < 2; ++l)
    {
        const juce::String p = l == 0 ? "lfo1" : "lfo2";
        s.lfo[l].shape = (int) get (p + "Shape");
        s.lfo[l].retrig = get (p + "Retrig") > 0.5f;
        clock.lfoSync[l] = get (p + "Sync") > 0.5f;
        clock.lfoDiv[l] = (int) get (p + "Div");
        s.lfo[l].rateHz = clock.lfoSync[l] ? (float) (clock.bpm / 60.0 / ws::syncDivBeats (clock.lfoDiv[l]))
                                           : get (p + "Rate");
    }

    for (int m = 0; m < ws::numMatrixSlots; ++m)
    {
        s.matrix[m].src = (int) get (ws::matrixId (m, "Src"));
        s.matrix[m].dst = (int) get (ws::matrixId (m, "Dst"));
        s.matrix[m].amt = get (ws::matrixId (m, "Amt"));
    }

    s.polyMode = (int) get ("polyMode");
    s.voices = (int) std::lround (get ("voices"));
    s.glide = get ("glide");
    s.bendRange = (int) std::lround (get ("bendRange"));

    clock.seqDiv = (int) get ("seqDiv");
    clock.seqLen = (int) std::lround (get ("seqLen"));
    clock.seqSmooth = get ("seqSmooth");
    for (int i = 0; i < ws::numSeqSteps; ++i)
        clock.seqSteps[i] = get (ws::seqStepId (i));

    clock.arpOn = get ("arpOn") > 0.5f;
    clock.arpMode = (int) get ("arpMode");
    clock.arpOct = (int) std::lround (get ("arpOct"));
    clock.arpDiv = (int) get ("arpDiv");
    clock.arpGate = get ("arpGate");

    fx.distOn = get ("distOn") > 0.5f;
    fx.distDrive = get ("distDrive");
    fx.distMix = get ("distMix");
    fx.chorusOn = get ("chorusOn") > 0.5f;
    fx.chorusRate = get ("chorusRate");
    fx.chorusDepth = get ("chorusDepth");
    fx.chorusMix = get ("chorusMix");
    fx.delayOn = get ("delayOn") > 0.5f;
    fx.delayBeats = ws::syncDivBeats ((int) get ("delayDiv"));
    fx.delayFb = get ("delayFb");
    fx.delayMix = get ("delayMix");
    fx.delayTone = get ("delayTone");
    fx.reverbOn = get ("reverbOn") > 0.5f;
    fx.reverbSize = get ("reverbSize");
    fx.reverbDamp = get ("reverbDamp");
    fx.reverbMix = get ("reverbMix");
    fx.reverbWidth = get ("reverbWidth");
    fx.masterGain = juce::Decibels::decibelsToGain (get ("masterVol"), -48.0f);
    fx.bpm = clock.bpm;
}

void WavesmithProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    updateSettings();
    engine.process (buffer, midi, settings, clock);

    if (buffer.getNumChannels() >= 2)
        effects.process (buffer, fx);
    else
        buffer.applyGain (fx.masterGain);

    midi.clear();
}

//==============================================================================
int WavesmithProcessor::getNumPrograms() { return std::max (1, presets.getNumPresets()); }
int WavesmithProcessor::getCurrentProgram() { return std::max (0, presets.getCurrentIndex()); }

void WavesmithProcessor::setCurrentProgram (int index)
{
    if (index == presets.getCurrentIndex()) return;
    presets.loadPreset (index);
    ++presetChangeCounter;
}

const juce::String WavesmithProcessor::getProgramName (int index)
{
    if (index < 0 || index >= presets.getNumPresets()) return "Init";
    const auto& p = presets.getPreset (index);
    return p.category + ": " + p.name;
}

void WavesmithProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetIndex", presets.getCurrentIndex(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void WavesmithProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            presets.setCurrentIndexSilently ((int) tree.getProperty ("presetIndex", -1));
            ++presetChangeCounter;
        }
    }
}

juce::AudioProcessorEditor* WavesmithProcessor::createEditor()
{
    return new WavesmithEditor (*this);
}

#ifndef WAVESMITH_TEST
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WavesmithProcessor();
}
#endif
