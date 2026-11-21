#include "SynthEngine.h"

namespace ws
{

void SynthEngine::prepare (double sr)
{
    sampleRate = sr;
    for (int i = 0; i < maxVoices; ++i)
        voices[(size_t) i].prepare (sr, 0x1234567u + (uint32_t) i * 7919u);
    reset();
}

void SynthEngine::reset()
{
    for (auto& v : voices) v.kill();
    heldKeys.clear();
    sustainedKeys.clear();
    monoStack.clear();
    sustainPedal = false;
    arpCurrentNote = -1;
    lastArpStep = -1.0;
    beatPos = 0.0;
    lastNotePitch = -1.0f;
}

//==============================================================================
void SynthEngine::voiceNoteOn (int note, float vel, const SynthSettings& s)
{
    if (s.polyMode != 0) // Mono / Legato
    {
        monoStack.erase (std::remove (monoStack.begin(), monoStack.end(), note), monoStack.end());
        const bool anyHeld = ! monoStack.empty();
        monoStack.push_back (note);

        auto& v = voices[0];
        for (size_t i = 1; i < voices.size(); ++i)
            if (voices[i].isActive()) voices[i].release();

        if (s.polyMode == 2 && anyHeld && v.isActive() && ! v.isReleased())
            v.legatoTo (note);
        else
            v.start (note, vel, lastNotePitch, s);
        lastNotePitch = (float) note;
        return;
    }

    const int numVoices = juce::jlimit (1, maxVoices, s.voices);
    Voice* target = nullptr;

    // Same note already sounding? retrigger it
    for (int i = 0; i < maxVoices && target == nullptr; ++i)
        if (voices[(size_t) i].isActive() && voices[(size_t) i].getNote() == note)
            target = &voices[(size_t) i];

    // Free voice
    for (int i = 0; i < numVoices && target == nullptr; ++i)
        if (! voices[(size_t) i].isActive())
            target = &voices[(size_t) i];

    // Steal: quietest released voice, otherwise the oldest
    if (target == nullptr)
    {
        float lowest = 10.0f;
        for (int i = 0; i < numVoices; ++i)
        {
            auto& v = voices[(size_t) i];
            if (v.isReleased() && v.getLevel() < lowest) { lowest = v.getLevel(); target = &v; }
        }
    }
    if (target == nullptr)
    {
        uint64_t oldest = UINT64_MAX;
        for (int i = 0; i < numVoices; ++i)
        {
            auto& v = voices[(size_t) i];
            if (v.getAge() < oldest) { oldest = v.getAge(); target = &v; }
        }
    }

    target->start (note, vel, lastNotePitch, s);
    lastNotePitch = (float) note;
}

void SynthEngine::voiceNoteOff (int note, const SynthSettings& s)
{
    if (s.polyMode != 0)
    {
        monoStack.erase (std::remove (monoStack.begin(), monoStack.end(), note), monoStack.end());
        auto& v = voices[0];
        if (v.isActive() && v.getNote() == note && ! v.isReleased())
        {
            if (! monoStack.empty())
            {
                v.legatoTo (monoStack.back());
                lastNotePitch = (float) monoStack.back();
            }
            else
                v.release();
        }
        return;
    }

    for (auto& v : voices)
        if (v.isActive() && v.getNote() == note && ! v.isReleased())
            v.release();
}

void SynthEngine::allVoicesOff()
{
    for (auto& v : voices)
        if (v.isActive()) v.release();
    monoStack.clear();
    heldKeys.clear();
    sustainedKeys.clear();
    arpCurrentNote = -1;
}

//==============================================================================
void SynthEngine::keyDown (int note, float vel, const SynthSettings& s, const ClockSettings& c)
{
    const bool wasEmpty = heldKeys.empty();
    heldKeys.erase (std::remove (heldKeys.begin(), heldKeys.end(), note), heldKeys.end());
    heldKeys.push_back (note);
    keyVelocity[(size_t) note] = vel;
    sustainedKeys.erase (std::remove (sustainedKeys.begin(), sustainedKeys.end(), note), sustainedKeys.end());

    if (wasEmpty && ! c.hostPlaying)
    {
        beatPos = 0.0;          // restart internal clock: arp, sequencer
        lastArpStep = -1.0;
        arpIndex = 0;
        arpDirection = 1;
        for (auto& p : freeLfoPhase) p = 0.0f;
    }

    if (! c.arpOn)
        voiceNoteOn (note, vel, s);
}

void SynthEngine::keyUp (int note, const SynthSettings& s, const ClockSettings& c)
{
    heldKeys.erase (std::remove (heldKeys.begin(), heldKeys.end(), note), heldKeys.end());

    if (c.arpOn)
    {
        if (heldKeys.empty() && arpCurrentNote >= 0)
        {
            voiceNoteOff (arpCurrentNote, s);
            arpCurrentNote = -1;
        }
        return;
    }

    if (sustainPedal)
        sustainedKeys.push_back (note);
    else
        voiceNoteOff (note, s);
}

void SynthEngine::handleMidi (const juce::MidiMessage& m, const SynthSettings& s, const ClockSettings& c)
{
    if (m.isNoteOn())
        keyDown (m.getNoteNumber(), m.getFloatVelocity(), s, c);
    else if (m.isNoteOff())
        keyUp (m.getNoteNumber(), s, c);
    else if (m.isPitchWheel())
        global.pitchBend = juce::jlimit (-1.0f, 1.0f, (float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        if (cc == 1)
            global.modWheel = (float) m.getControllerValue() / 127.0f;
        else if (cc == 64)
        {
            const bool down = m.getControllerValue() >= 64;
            if (sustainPedal && ! down)
            {
                for (int n : sustainedKeys)
                    if (std::find (heldKeys.begin(), heldKeys.end(), n) == heldKeys.end())
                        voiceNoteOff (n, s);
                sustainedKeys.clear();
            }
            sustainPedal = down;
        }
        else if (cc == 120 || cc == 123)
            allVoicesOff();
    }
    else if (m.isChannelPressure())
        global.aftertouch = (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch())
        global.aftertouch = (float) m.getAfterTouchValue() / 127.0f;
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        allVoicesOff();
}

//==============================================================================
int SynthEngine::nextArpNote (const ClockSettings& c)
{
    std::vector<int> base = heldKeys;
    if (c.arpMode != 4)
        std::sort (base.begin(), base.end());

    std::vector<int> notes;
    for (int o = 0; o < juce::jlimit (1, 4, c.arpOct); ++o)
        for (int n : base)
            if (n + o * 12 < 128) notes.push_back (n + o * 12);

    const int size = (int) notes.size();
    if (size == 0) return -1;

    int idx = 0;
    switch (c.arpMode)
    {
        case 1: // down
            idx = (size - 1) - (arpIndex % size);
            ++arpIndex;
            break;
        case 2: // up / down
        {
            if (size == 1) { idx = 0; break; }
            const int period = 2 * size - 2;
            const int p = arpIndex % period;
            idx = p < size ? p : period - p;
            ++arpIndex;
            break;
        }
        case 3: // random
            idx = random.nextInt (size);
            break;
        default: // up, as played
            idx = arpIndex % size;
            ++arpIndex;
            break;
    }
    return notes[(size_t) idx];
}

void SynthEngine::advanceClock (int numSamples, const SynthSettings& s, const ClockSettings& c)
{
    // --- arpeggiator events at the current position
    if (! c.arpOn && arpCurrentNote >= 0)
    {
        voiceNoteOff (arpCurrentNote, s);
        arpCurrentNote = -1;
    }

    if (c.arpOn)
    {
        const double stepLen = syncDivBeats (c.arpDiv);
        if (arpCurrentNote >= 0 && beatPos >= arpNoteOffBeat)
        {
            voiceNoteOff (arpCurrentNote, s);
            arpCurrentNote = -1;
        }

        const double step = std::floor (beatPos / stepLen + 1.0e-9);
        if (step != lastArpStep)
        {
            lastArpStep = step;
            if (! heldKeys.empty())
            {
                if (arpCurrentNote >= 0)
                    voiceNoteOff (arpCurrentNote, s);
                const int n = nextArpNote (c);
                if (n >= 0)
                {
                    const int origin = heldKeys.back();
                    voiceNoteOn (n, keyVelocity[(size_t) juce::jlimit (0, 127, origin)], s);
                    arpCurrentNote = n;
                    arpNoteOffBeat = (step + juce::jlimit (0.05f, 1.0f, c.arpGate) * 0.999) * stepLen;
                }
            }
        }
    }

    // --- step sequencer value
    {
        const double stepLen = syncDivBeats (c.seqDiv);
        const int len = juce::jlimit (1, numSeqSteps, c.seqLen);
        const double pos = beatPos / stepLen;
        const double fl = std::floor (pos);
        const int idx = (int) ((int64_t) fl % len);
        const int nextIdx = (idx + 1) % len;
        const float frac = (float) (pos - fl);
        float value = c.seqSteps[idx];
        if (c.seqSmooth > 0.0f)
        {
            const float startX = 1.0f - c.seqSmooth;
            if (frac > startX)
            {
                float w = (frac - startX) / c.seqSmooth;
                w = w * w * (3.0f - 2.0f * w);
                value += (c.seqSteps[nextIdx] - value) * w;
            }
        }
        global.seqValue = value;
        currentSeqStep.store (idx, std::memory_order_relaxed);
    }

    // --- free running LFO phases (for non-retriggered LFOs)
    for (int l = 0; l < 2; ++l)
    {
        float newPhase;
        if (c.lfoSync[l] && c.hostPlaying)
        {
            const double p = beatPos / syncDivBeats (c.lfoDiv[l]);
            newPhase = (float) (p - std::floor (p));
        }
        else
        {
            newPhase = freeLfoPhase[l] + s.lfo[l].rateHz * (float) numSamples / (float) sampleRate;
            newPhase -= std::floor (newPhase);
        }
        if (newPhase < freeLfoPhase[l])
        {
            global.lfoPrevHeld[l] = global.lfoHeld[l];
            global.lfoHeld[l] = random.nextFloat() * 2.0f - 1.0f;
        }
        freeLfoPhase[l] = newPhase;
        global.lfoPhase[l] = newPhase;
    }

    beatPos += (double) numSamples * c.bpm / 60.0 / sampleRate;
}

//==============================================================================
void SynthEngine::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi,
                           const SynthSettings& s, const ClockSettings& c)
{
    const int numSamples = buffer.getNumSamples();
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    if (c.hostPlaying)
    {
        if (! wasHostPlaying)
            lastArpStep = -1.0;
        beatPos = c.hostPpq;
    }
    wasHostPlaying = c.hostPlaying;

    float tmpR[chunkSize];
    auto it = midi.begin();
    int pos = 0;

    while (pos < numSamples)
    {
        // events due now
        while (it != midi.end() && (*it).samplePosition <= pos)
        {
            handleMidi ((*it).getMessage(), s, c);
            ++it;
        }

        int end = std::min (numSamples, pos + chunkSize);
        if (it != midi.end())
            end = std::min (end, std::max (pos + 1, (*it).samplePosition));
        const int n = end - pos;

        advanceClock (n, s, c);

        float* r = outR != nullptr ? outR + pos : tmpR;
        if (outR == nullptr) std::fill (tmpR, tmpR + n, 0.0f);
        for (auto& v : voices)
            v.render (outL + pos, r, n, s, global);

        pos = end;
    }

    // remaining events (positions beyond the block should not happen)
    for (; it != midi.end(); ++it)
        handleMidi ((*it).getMessage(), s, c);
}

} // namespace ws
