#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace pf;

namespace
{
    const juce::StringArray rateNames  { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    const double rateQuarters[]        { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };

    const juce::StringArray delayNames { "1/16", "1/8T", "1/8", "1/4T", "1/8D", "1/4", "1/4D", "1/2" };
    const double delayQuarters[]       { 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 0.75, 1.0, 1.5, 2.0 };

    struct Preset
    {
        const char* name;
        std::array<float, kNumSteps> gate, filt, pan;
        std::vector<std::pair<const char*, float>> values;
    };

    std::vector<Preset> makePresets()
    {
        std::vector<Preset> p;
        const std::array<float, kNumSteps> ones  { 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1 };
        const std::array<float, kNumSteps> zeros { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 };
        const std::array<float, kNumSteps> half  { .5f,.5f,.5f,.5f, .5f,.5f,.5f,.5f, .5f,.5f,.5f,.5f, .5f,.5f,.5f,.5f };

        p.push_back ({ "Init", ones, half, half,
                       { { "gateOn", 0 }, { "filtOn", 0 }, { "panOn", 0 }, { "delayOn", 0 }, { "revOn", 0 },
                         { "drive", 0 }, { "bits", 16 }, { "downsample", 1 }, { "mix", 1 } } });

        p.push_back ({ "Trance Gate",
                       { 1,0,1,1, 0,1,1,0, 1,0,1,1, 0,1,0,1 }, half, half,
                       { { "gateOn", 1 }, { "gateDepth", 1 }, { "gateLen", 0.75f }, { "gateSmooth", 3 }, { "rate", 3 },
                         { "filtOn", 0 }, { "panOn", 0 }, { "delayOn", 1 }, { "delayTime", 4 }, { "delayFb", 0.35f },
                         { "delayPing", 1 }, { "delayMix", 0.25f }, { "revOn", 1 }, { "revSize", 0.6f }, { "revMix", 0.15f },
                         { "drive", 0 }, { "bits", 16 }, { "downsample", 1 }, { "mix", 1 } } });

        p.push_back ({ "Filter Walk", ones,
                       { 0.1f,0.3f,0.6f,0.9f, 0.2f,0.5f,0.8f,0.4f, 0.1f,0.7f,0.3f,1.0f, 0.5f,0.2f,0.9f,0.6f }, half,
                       { { "gateOn", 0 }, { "filtOn", 1 }, { "filtType", 0 }, { "cutoff", 250 }, { "reso", 0.65f },
                         { "filtDepth", 5 }, { "filtGlide", 15 }, { "rate", 3 }, { "panOn", 0 }, { "delayOn", 0 },
                         { "revOn", 0 }, { "drive", 0.2f }, { "bits", 16 }, { "downsample", 1 }, { "mix", 1 } } });

        p.push_back ({ "Stutter Pan",
                       { 1,1,0,1, 1,0,1,1, 1,1,0,1, 0,1,1,1 }, half,
                       { 0,1,0,1, 0.2f,0.8f,0.2f,0.8f, 0,1,0,1, 0.5f,0,1,0.5f },
                       { { "gateOn", 1 }, { "gateDepth", 1 }, { "gateLen", 0.5f }, { "gateSmooth", 1 }, { "rate", 5 },
                         { "filtOn", 0 }, { "panOn", 1 }, { "panDepth", 1 }, { "delayOn", 0 }, { "revOn", 0 },
                         { "drive", 0 }, { "bits", 16 }, { "downsample", 1 }, { "mix", 1 } } });

        p.push_back ({ "Dub Space",
                       { 1,0,0,1, 0,0,1,0, 1,0,0,1, 0,0,1,0 },
                       { 0.8f,0.6f,0.4f,0.2f, 0.8f,0.6f,0.4f,0.2f, 0.8f,0.6f,0.4f,0.2f, 0.8f,0.6f,0.4f,0.2f }, half,
                       { { "gateOn", 1 }, { "gateDepth", 0.85f }, { "gateLen", 0.6f }, { "gateSmooth", 6 }, { "rate", 3 },
                         { "filtOn", 1 }, { "filtType", 1 }, { "cutoff", 400 }, { "reso", 0.5f }, { "filtDepth", 3 },
                         { "filtGlide", 40 }, { "panOn", 0 }, { "delayOn", 1 }, { "delayTime", 4 }, { "delayFb", 0.65f },
                         { "delayPing", 1 }, { "delayMix", 0.5f }, { "revOn", 1 }, { "revSize", 0.85f }, { "revDamp", 0.6f },
                         { "revMix", 0.3f }, { "drive", 0.1f }, { "bits", 16 }, { "downsample", 1 }, { "mix", 1 } } });

        p.push_back ({ "Lo-Fi Chop",
                       { 1,1,1,0, 1,1,0,1, 1,0,1,1, 1,1,1,0 },
                       { 1,0.4f,0.7f,0.2f, 1,0.4f,0.7f,0.2f, 1,0.4f,0.7f,0.2f, 1,0.4f,0.7f,0.2f }, half,
                       { { "gateOn", 1 }, { "gateDepth", 1 }, { "gateLen", 0.8f }, { "gateSmooth", 2 }, { "rate", 3 },
                         { "filtOn", 1 }, { "filtType", 0 }, { "cutoff", 600 }, { "reso", 0.3f }, { "filtDepth", 4 },
                         { "filtGlide", 5 }, { "panOn", 0 }, { "delayOn", 0 }, { "revOn", 1 }, { "revSize", 0.4f },
                         { "revMix", 0.12f }, { "drive", 0.45f }, { "bits", 8 }, { "downsample", 6 }, { "mix", 1 } } });

        juce::ignoreUnused (zeros);
        return p;
    }

    const std::vector<Preset>& presets()
    {
        static const auto p = makePresets();
        return p;
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PulseForgeProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&] (const String& id, const String& name, NormalisableRange<float> range, float def,
                         const String& label = {})
    {
        const int decimals = range.end >= 1000.0f ? 0 : (range.end > 20.0f ? 1 : 2);
        auto toText = [decimals, label] (float v, int)
        {
            if (label == "Hz" && v >= 1000.0f)
                return String (v / 1000.0f, 2) + " kHz";
            return String (v, decimals) + (label.isEmpty() ? String() : " " + label);
        };
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes()
                                                               .withLabel (label)
                                                               .withStringFromValueFunction (toText)));
    };
    auto addBool = [&] (const String& id, const String& name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto addChoice = [&] (const String& id, const String& name, const StringArray& choices, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, def));
    };

    // Sequencer
    addChoice ("rate", "Step Rate", rateNames, 3);
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "length", 1 }, "Steps", 1, kNumSteps, kNumSteps));
    addFloat ("swing", "Swing", { 0.0f, 0.5f }, 0.0f);

    // Step values
    const float gateDefaults[kNumSteps] { 1,0,1,1, 0,1,1,0, 1,0,1,1, 0,1,0,1 };
    for (int i = 0; i < kNumSteps; ++i)
        addFloat (stepId (Lane::Gate, i), "Gate Step " + String (i + 1), { 0.0f, 1.0f }, gateDefaults[i]);
    for (int i = 0; i < kNumSteps; ++i)
        addFloat (stepId (Lane::Filter, i), "Filter Step " + String (i + 1), { 0.0f, 1.0f }, 0.5f);
    for (int i = 0; i < kNumSteps; ++i)
        addFloat (stepId (Lane::Pan, i), "Pan Step " + String (i + 1), { 0.0f, 1.0f }, 0.5f);

    // Gate
    addBool ("gateOn", "Gate On", true);
    addFloat ("gateDepth", "Gate Depth", { 0.0f, 1.0f }, 1.0f);
    addFloat ("gateLen", "Gate Length", { 0.05f, 1.0f }, 0.75f);
    addFloat ("gateSmooth", "Gate Smooth", { 0.2f, 50.0f, 0.0f, 0.4f }, 3.0f, "ms");

    // Filter
    addBool ("filtOn", "Filter On", false);
    addChoice ("filtType", "Filter Type", { "Low Pass", "Band Pass", "High Pass" }, 0);
    addFloat ("cutoff", "Cutoff", { 20.0f, 20000.0f, 0.0f, 0.25f }, 800.0f, "Hz");
    addFloat ("reso", "Resonance", { 0.0f, 1.0f }, 0.4f);
    addFloat ("filtDepth", "Filter Seq Depth", { -6.0f, 6.0f }, 3.0f, "oct");
    addFloat ("filtGlide", "Filter Glide", { 0.5f, 200.0f, 0.0f, 0.4f }, 10.0f, "ms");

    // Pan
    addBool ("panOn", "Pan On", false);
    addFloat ("panDepth", "Pan Depth", { 0.0f, 1.0f }, 1.0f);

    // Character
    addFloat ("drive", "Drive", { 0.0f, 1.0f }, 0.0f);
    addFloat ("bits", "Bit Depth", { 2.0f, 16.0f }, 16.0f, "bit");
    addFloat ("downsample", "Downsample", { 1.0f, 32.0f, 0.0f, 0.5f }, 1.0f, "x");

    // Delay
    addBool ("delayOn", "Delay On", false);
    addChoice ("delayTime", "Delay Time", delayNames, 4);
    addFloat ("delayFb", "Delay Feedback", { 0.0f, 0.95f }, 0.4f);
    addBool ("delayPing", "Ping Pong", true);
    addFloat ("delayMix", "Delay Mix", { 0.0f, 1.0f }, 0.3f);

    // Reverb
    addBool ("revOn", "Reverb On", false);
    addFloat ("revSize", "Reverb Size", { 0.0f, 1.0f }, 0.6f);
    addFloat ("revDamp", "Reverb Damping", { 0.0f, 1.0f }, 0.5f);
    addFloat ("revMix", "Reverb Mix", { 0.0f, 1.0f }, 0.2f);

    // Master
    addFloat ("mix", "Dry/Wet", { 0.0f, 1.0f }, 1.0f);
    addFloat ("out", "Output", { -24.0f, 12.0f }, 0.0f, "dB");

    return layout;
}

//==============================================================================
PulseForgeProcessor::PulseForgeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int l = 0; l < 3; ++l)
        for (int i = 0; i < kNumSteps; ++i)
            steps[(size_t) l][(size_t) i] = apvts.getRawParameterValue (stepId ((Lane) l, i));

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pRate = get ("rate");           pLength = get ("length");        pSwing = get ("swing");
    pGateOn = get ("gateOn");       pGateDepth = get ("gateDepth");  pGateLen = get ("gateLen");
    pGateSmooth = get ("gateSmooth");
    pFiltOn = get ("filtOn");       pFiltType = get ("filtType");    pCutoff = get ("cutoff");
    pReso = get ("reso");           pFiltDepth = get ("filtDepth");  pFiltGlide = get ("filtGlide");
    pPanOn = get ("panOn");         pPanDepth = get ("panDepth");
    pDrive = get ("drive");         pBits = get ("bits");            pDownsample = get ("downsample");
    pDelayOn = get ("delayOn");     pDelayTime = get ("delayTime");  pDelayFb = get ("delayFb");
    pDelayPing = get ("delayPing"); pDelayMix = get ("delayMix");
    pRevOn = get ("revOn");         pRevSize = get ("revSize");      pRevDamp = get ("revDamp");
    pRevMix = get ("revMix");
    pMix = get ("mix");             pOut = get ("out");
}

bool PulseForgeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void PulseForgeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (auto& f : filters) f.reset();
    gateEnv = 1.0f; filtMod = 0.5f; panMod = 0.5f;
    crushHold = { 0.0f, 0.0f }; crushCounter = 0.0f;

    delayBuf.setSize (2, (int) (sampleRate * 4.5) + 4);
    delayBuf.clear();
    delayWrite = 0;
    delayLP = { 0.0f, 0.0f };
    delayTimeSmoothed.reset (sampleRate, 0.08);
    delayTimeSmoothed.setCurrentAndTargetValue ((float) (sampleRate * 0.375));

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    dryBuf.setSize (2, samplesPerBlock);
    wetTmp.setSize (2, samplesPerBlock);
}

//==============================================================================
void PulseForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || buffer.getNumChannels() < 2)
        return;

    // Mono input -> duplicate to right channel
    if (getTotalNumInputChannels() == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    if (dryBuf.getNumSamples() < numSamples)
    {
        dryBuf.setSize (2, numSamples, false, false, true);
        wetTmp.setSize (2, numSamples, false, false, true);
    }
    dryBuf.copyFrom (0, 0, buffer, 0, 0, numSamples);
    dryBuf.copyFrom (1, 0, buffer, 1, 0, numSamples);

    // ---- Transport ---------------------------------------------------------
    double bpm = lastBpm;
    double ppq = ppqFree;
    bool hostPlaying = false;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 999.0, *b);
            hostPlaying = pos->getIsPlaying();
            if (hostPlaying)
                if (auto q = pos->getPpqPosition())
                    ppq = *q;
        }
    }
    lastBpm = bpm;

    const double ppqPerSample = bpm / 60.0 / sr;
    const int    rateIdx  = juce::jlimit (0, 5, (int) pRate->load());
    const double stepLen  = rateQuarters[rateIdx];
    const int    numSteps = juce::jlimit (1, kNumSteps, (int) pLength->load());
    const double swing    = pSwing->load();

    const bool  gateOn    = pGateOn->load() > 0.5f;
    const float gateDepth = pGateDepth->load();
    const float gateLen   = pGateLen->load();
    const float gateCoef  = 1.0f - std::exp (-1.0f / (0.001f * pGateSmooth->load() * (float) sr));

    const bool  filtOn    = pFiltOn->load() > 0.5f;
    const int   filtType  = (int) pFiltType->load();
    const float cutoff    = pCutoff->load();
    const float k         = 2.0f * (1.0f - 0.97f * pReso->load());
    const float filtDepth = pFiltDepth->load();
    const float filtCoef  = 1.0f - std::exp (-1.0f / (0.001f * pFiltGlide->load() * (float) sr));
    const float nyq       = (float) sr * 0.45f;

    const bool  panOn     = pPanOn->load() > 0.5f;
    const float panDepth  = pPanDepth->load();
    const float panCoef   = 1.0f - std::exp (-1.0f / (0.004f * (float) sr));

    const float drive     = pDrive->load();
    const float driveGain = 1.0f + drive * drive * 30.0f;
    const float driveComp = 1.0f / std::tanh (std::min (driveGain, 3.0f));
    const float bits      = pBits->load();
    const bool  crushOn   = bits < 15.9f;
    const float levels    = std::pow (2.0f, bits - 1.0f);
    const float dsFactor  = pDownsample->load();
    const bool  dsOn      = dsFactor > 1.01f;

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    int lastStep = currentStep.load();

    for (int n = 0; n < numSamples; ++n)
    {
        // ---- step position with swing ----
        const double pos      = ppq + n * ppqPerSample;
        const double stepsF   = pos / stepLen;
        const double pairIdx  = std::floor (stepsF * 0.5);
        const double inPair   = stepsF - 2.0 * pairIdx;   // 0..2
        const double split    = 1.0 + swing;
        long long absStep;
        double phase;
        if (inPair < split) { absStep = (long long) pairIdx * 2;     phase = inPair / split; }
        else                { absStep = (long long) pairIdx * 2 + 1; phase = (inPair - split) / (2.0 - split); }
        int stepIdx = (int) (((absStep % numSteps) + numSteps) % numSteps);
        lastStep = stepIdx;

        float l = L[n], r = R[n];

        // ---- drive ----
        if (drive > 0.001f)
        {
            l = std::tanh (l * driveGain) * driveComp / (1.0f + drive * 1.5f);
            r = std::tanh (r * driveGain) * driveComp / (1.0f + drive * 1.5f);
        }

        // ---- bitcrush / downsample ----
        if (dsOn)
        {
            crushCounter += 1.0f;
            if (crushCounter >= dsFactor)
            {
                crushCounter -= dsFactor;
                crushHold = { l, r };
            }
            l = crushHold[0];
            r = crushHold[1];
        }
        if (crushOn)
        {
            l = std::round (l * levels) / levels;
            r = std::round (r * levels) / levels;
        }

        // ---- step filter ----
        if (filtOn)
        {
            const float target = steps[1][(size_t) stepIdx]->load();
            filtMod += filtCoef * (target - filtMod);
            const float fc = juce::jlimit (20.0f, nyq, cutoff * std::exp2 ((filtMod - 0.5f) * 2.0f * filtDepth));
            const float g  = std::tan (juce::MathConstants<float>::pi * fc / (float) sr);
            l = filters[0].process (l, g, k, filtType);
            r = filters[1].process (r, g, k, filtType);
        }

        // ---- step gate ----
        if (gateOn)
        {
            const float stepVal = steps[0][(size_t) stepIdx]->load();
            const float target  = (phase < gateLen) ? stepVal : 0.0f;
            const float level   = 1.0f - gateDepth * (1.0f - target);
            gateEnv += gateCoef * (level - gateEnv);
            l *= gateEnv;
            r *= gateEnv;
        }

        // ---- step pan ----
        if (panOn)
        {
            const float target = steps[2][(size_t) stepIdx]->load();
            panMod += panCoef * (target - panMod);
            const float p = (panMod * 2.0f - 1.0f) * panDepth; // -1..1
            const float half = juce::MathConstants<float>::halfPi;
            const float gl = p > 0.0f ? std::cos (p * half) : 1.0f;
            const float gr = p < 0.0f ? std::cos (-p * half) : 1.0f;
            // fold opposite channel in so hard pans keep energy
            const float nl = l * gl + (p < 0.0f ? r * (1.0f - gr) : 0.0f);
            const float nr = r * gr + (p > 0.0f ? l * (1.0f - gl) : 0.0f);
            l = nl; r = nr;
        }

        L[n] = l;
        R[n] = r;
    }

    currentStep.store (lastStep);

    // ---- delay ----
    if (pDelayOn->load() > 0.5f)
    {
        const int   dIdx = juce::jlimit (0, 7, (int) pDelayTime->load());
        const float dSamples = (float) juce::jlimit (1.0, (double) delayBuf.getNumSamples() - 3.0,
                                                     delayQuarters[dIdx] * 60.0 / bpm * sr);
        delayTimeSmoothed.setTargetValue (dSamples);
        const float fb   = pDelayFb->load();
        const bool  ping = pDelayPing->load() > 0.5f;
        const float dmix = pDelayMix->load();
        const int   size = delayBuf.getNumSamples();
        auto* dl = delayBuf.getWritePointer (0);
        auto* dr = delayBuf.getWritePointer (1);
        const float lpCoef = 0.35f;

        for (int n = 0; n < numSamples; ++n)
        {
            const float d = delayTimeSmoothed.getNextValue();
            float readPos = (float) delayWrite - d;
            if (readPos < 0.0f) readPos += (float) size;
            const int   i0 = (int) readPos;
            const int   i1 = (i0 + 1) % size;
            const float fr = readPos - (float) i0;
            float yl = dl[i0] + fr * (dl[i1] - dl[i0]);
            float yr = dr[i0] + fr * (dr[i1] - dr[i0]);

            delayLP[0] += lpCoef * (yl - delayLP[0]);
            delayLP[1] += lpCoef * (yr - delayLP[1]);
            const float fl = delayLP[0], frr = delayLP[1];

            const float inL = L[n], inR = R[n];
            if (ping)
            {
                dl[delayWrite] = std::tanh (0.5f * (inL + inR) + fb * frr);
                dr[delayWrite] = std::tanh (fb * fl);
            }
            else
            {
                dl[delayWrite] = std::tanh (inL + fb * fl);
                dr[delayWrite] = std::tanh (inR + fb * frr);
            }
            if (++delayWrite >= size) delayWrite = 0;

            L[n] = inL + dmix * yl;
            R[n] = inR + dmix * yr;
        }
    }
    else
    {
        delayBuf.clear();
    }

    // ---- reverb ----
    if (pRevOn->load() > 0.5f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize   = pRevSize->load();
        rp.damping    = pRevDamp->load();
        rp.wetLevel   = 1.0f;
        rp.dryLevel   = 0.0f;
        rp.width      = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);

        wetTmp.copyFrom (0, 0, buffer, 0, 0, numSamples);
        wetTmp.copyFrom (1, 0, buffer, 1, 0, numSamples);
        reverb.processStereo (wetTmp.getWritePointer (0), wetTmp.getWritePointer (1), numSamples);

        const float rmix = pRevMix->load();
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* o = buffer.getWritePointer (ch);
            auto* w = wetTmp.getReadPointer (ch);
            for (int n = 0; n < numSamples; ++n)
                o[n] = o[n] * (1.0f - 0.5f * rmix) + w[n] * rmix;
        }
    }

    // ---- dry/wet + output ----
    const float mix = pMix->load();
    const float outGain = juce::Decibels::decibelsToGain (pOut->load());
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* o = buffer.getWritePointer (ch);
        auto* d = dryBuf.getReadPointer (ch);
        for (int n = 0; n < numSamples; ++n)
            o[n] = (d[n] * (1.0f - mix) + o[n] * mix) * outGain;
    }

    // advance free-running clock
    ppqFree = hostPlaying ? ppq + numSamples * ppqPerSample : ppqFree + numSamples * ppqPerSample;
    if (ppqFree > 1.0e6) ppqFree = 0.0;
}

//==============================================================================
void PulseForgeProcessor::randomiseLane (Lane lane)
{
    juce::Random rnd;
    for (int i = 0; i < kNumSteps; ++i)
    {
        if (auto* p = apvts.getParameter (stepId (lane, i)))
        {
            float v = rnd.nextFloat();
            if (lane == Lane::Gate) v = v > 0.4f ? 1.0f : 0.0f;
            p->setValueNotifyingHost (v);
        }
    }
}

int PulseForgeProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String PulseForgeProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presets().size()))
        return presets()[(size_t) index].name;
    return {};
}

void PulseForgeProcessor::setCurrentProgram (int index) { applyPreset (index); }

void PulseForgeProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size()))
        return;

    currentPreset = index;
    const auto& pr = presets()[(size_t) index];

    auto set = [this] (const juce::String& id, float plainValue)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
    };

    for (int i = 0; i < kNumSteps; ++i)
    {
        set (stepId (Lane::Gate, i),   pr.gate[(size_t) i]);
        set (stepId (Lane::Filter, i), pr.filt[(size_t) i]);
        set (stepId (Lane::Pan, i),    pr.pan[(size_t) i]);
    }
    for (const auto& [id, v] : pr.values)
        set (id, v);
}

//==============================================================================
void PulseForgeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PulseForgeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentPreset = tree.getProperty ("preset", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* PulseForgeProcessor::createEditor() { return new PulseForgeEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PulseForgeProcessor(); }
