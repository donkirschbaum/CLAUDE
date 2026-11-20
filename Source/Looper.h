// LoopTape - Tape Echo + Layer Looper
// Copyright (C) 2026 donkirschbaum - AGPL-3.0 (see LICENSE)
//
// Multi-layer looper. JUCE-free, real-time safe on the audio thread.
//
//  * Layer 0 is the base loop (defines the loop length).
//  * Every overdub goes into its OWN layer, so you can undo / redo
//    layer by layer (up to kMaxLayers).
//  * Layers 1..N are allocated on the message thread (allocateExtraLayers)
//    once the loop length is known; the audio thread never allocates.
//  * A fresh overdub layer is never memset on the audio thread: each layer
//    tracks which part of it has been written in the current take, and
//    unwritten samples count as silence.
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace lt
{
class Looper
{
public:
    enum class State : int { Empty = 0, Recording, Playing, Overdubbing, Stopped };

    static constexpr int   kMaxLayers  = 16;
    static constexpr float kMaxSeconds = 60.0f;

    //==================================================================== setup
    // not concurrent with processing (prepareToPlay)
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        maxLen = (int64_t) (sampleRate * kMaxSeconds);
        for (int c = 0; c < 2; ++c) layers[0].data[c].assign ((size_t) maxLen, 0.0f);
        fadeLen = std::max (16, (int) (sampleRate * 0.004));        // 4 ms seam fades
        rampStep = 1.0f / (float) std::max (1.0, sampleRate * 0.008); // 8 ms gain ramps
        resetAll();
        loopId.fetch_add (1);
    }

    // message thread, call periodically (timer). Allocates overdub layers once
    // a new loop length has been set by the audio thread.
    void allocateExtraLayers()
    {
        const uint32_t id = loopId.load (std::memory_order_acquire);
        if (id == allocatedFor.load (std::memory_order_acquire)) return;
        const int64_t len = requestedLen.load (std::memory_order_acquire);
        if (len > 0)
            for (int k = 1; k < kMaxLayers; ++k)
                for (int c = 0; c < 2; ++c)
                    layers[k].data[c].assign ((size_t) len, 0.0f);
        allocatedFor.store (id, std::memory_order_release);
    }

    //================================================================= commands
    // all commands are called from the audio thread
    void pressRecord()
    {
        if (clearing) finishClear();

        switch (state)
        {
            case State::Empty:
                startFirstRecording();
                break;

            case State::Recording:
                closeFirstRecording (State::Playing);
                break;

            case State::Playing:
            case State::Stopped:
                if (state == State::Stopped) startPlayback();
                startOverdub();
                break;

            case State::Overdubbing:
                stopOverdub();
                state = State::Playing;
                break;
        }
    }

    void pressPlayStop()
    {
        if (clearing) return;
        switch (state)
        {
            case State::Empty: break;
            case State::Recording:   closeFirstRecording (State::Stopped); break;
            case State::Overdubbing: stopOverdub(); [[fallthrough]];
            case State::Playing:     playTarget = 0.0f; state = State::Stopped; pendingDub = false; break;
            case State::Stopped:     startPlayback(); state = State::Playing; break;
        }
    }

    void pressUndo()
    {
        if (clearing) return;
        if (state == State::Recording) { discardAll(); return; }
        if (pendingDub) { pendingDub = false; if (state == State::Overdubbing) state = State::Playing; return; }

        if (dubLayer >= 0 && state == State::Overdubbing)
        {
            // throw away the take in progress (not redo-able)
            const int k = dubLayer;
            dubLayer = -1; recGain = 0.0f; recTarget = 0.0f;
            numLayers = k; numStored = k;
            state = State::Playing;
            return;
        }
        if (numLayers > 1)
        {
            if (dubLayer == numLayers - 1) finalizeDub();
            --numLayers;
        }
    }

    void pressRedo()
    {
        if (clearing || state == State::Recording || state == State::Overdubbing) return;
        if (numLayers < numStored) ++numLayers;
    }

    void pressClear()
    {
        if (state == State::Empty) return;
        if (state == State::Recording) { discardAll(); return; }
        clearing = true;
        playTarget = 0.0f;
        recTarget = 0.0f;
    }

    void setReverse (bool r)
    {
        const int d = r ? -1 : 1;
        if (d == dir) return;
        if (state == State::Overdubbing) { stopOverdub(); finalizeDub(); state = State::Playing; }
        dir = d;
    }

    void setHalfSpeed (bool h) { rate = h ? 0.5 : 1.0; }

    // length unit (samples) the base loop gets rounded to. 0 = free length
    void setQuantizeUnit (double samples) { quantUnit = samples; }

    //=============================================================== processing
    // 1) read what the loop plays at the current position
    void read (float& outL, float& outR)
    {
        outL = outR = 0.0f;
        if (loopLen <= 0) return;

        const bool extrasOk = extrasReady();
        const int64_t i0 = (int64_t) pos;
        const float frac = (float) (pos - (double) i0);
        int64_t i1 = i0 + dir;
        if (i1 >= loopLen) i1 -= loopLen;
        if (i1 < 0) i1 += loopLen;

        for (int k = 0; k < kMaxLayers; ++k)
        {
            float& g = layerGain[k];
            const float target = k < numLayers ? 1.0f : 0.0f;
            if (g != target) g = target > g ? std::min (target, g + rampStep) : std::max (target, g - rampStep);
            if (g <= 0.0f) continue;
            if (k > 0 && ! extrasOk) continue;

            const Layer& L = layers[k];
            float aL = 0, aR = 0, bL = 0, bR = 0;
            if (isWritten (L, i0)) { aL = L.data[0][(size_t) i0]; aR = L.data[1][(size_t) i0]; }
            if (frac > 0.0f && isWritten (L, i1)) { bL = L.data[0][(size_t) i1]; bR = L.data[1][(size_t) i1]; }
            outL += g * (aL + (bL - aL) * frac);
            outR += g * (aR + (bR - aR) * frac);
        }

        // transport gain (play / stop / clear fades)
        if (playGain != playTarget)
            playGain = playTarget > playGain ? std::min (playTarget, playGain + rampStep)
                                             : std::max (playTarget, playGain - rampStep);
        outL *= playGain;
        outR *= playGain;
    }

    // 2) record (if recording / overdubbing) and advance the playhead
    void writeAndAdvance (float inL, float inR)
    {
        if (clearing)
        {
            if (playGain <= 0.0f) finishClear();
            else advance();
            return;
        }

        if (pendingDub && extrasReady() && state != State::Empty) { pendingDub = false; beginDubLayer(); }

        switch (state)
        {
            case State::Recording:
            {
                rampRec();
                const float rampIn = recLen < fadeLen ? (float) recLen / (float) fadeLen : 1.0f;
                layers[0].data[0][(size_t) recLen] = inL * rampIn;
                layers[0].data[1][(size_t) recLen] = inR * rampIn;
                ++recLen;
                if (recLen >= maxLen || (recStopAt > 0 && recLen >= recStopAt))
                    finishFirstRecording();
                return;
            }
            case State::Empty:
                return;
            default:
                break;
        }

        if (dubLayer >= 0)
        {
            rampRec();
            const int64_t idx = (int64_t) pos;
            if (idx != lastWriteIdx)
            {
                Layer& L = layers[dubLayer];
                const float g = recGain;
                if (isWritten (L, idx))
                {
                    L.data[0][(size_t) idx] += inL * g;
                    L.data[1][(size_t) idx] += inR * g;
                }
                else
                {
                    L.data[0][(size_t) idx] = inL * g;
                    L.data[1][(size_t) idx] = inR * g;
                    if (L.written < loopLen) ++L.written;
                }
                lastWriteIdx = idx;
            }
            if (recTarget <= 0.0f && recGain <= 0.0f) finalizeDub();
        }

        advance();
    }

    //=================================================================== info
    State   getState() const      { return state; }
    int     getNumLayers() const  { return numLayers; }
    int     getNumStored() const  { return numStored; }
    int64_t getLoopLength() const { return loopLen; }
    int64_t getRecLength() const  { return recLen; }
    int64_t getRecTarget() const  { return recStopAt; }
    double  getPosition() const   { return pos; }
    bool    isPendingDub() const  { return pendingDub; }
    bool    isClearing() const    { return clearing; }
    double  getSampleRate() const { return sr; }

private:
    struct Layer
    {
        std::vector<float> data[2];
        int64_t start = 0;   // index where the current take started
        int     dir = 1;     // playback direction during the take
        int64_t written = 0; // number of indices written so far in this take
    };

    bool extrasReady() const
    {
        return allocatedFor.load (std::memory_order_acquire) == loopId.load (std::memory_order_relaxed);
    }

    bool isWritten (const Layer& L, int64_t idx) const
    {
        if (L.written >= loopLen) return true;
        if (L.written <= 0) return false;
        int64_t d = L.dir > 0 ? idx - L.start : L.start - idx;
        if (d < 0) d += loopLen;
        return d < L.written;
    }

    void rampRec()
    {
        if (recGain != recTarget)
            recGain = recTarget > recGain ? std::min (recTarget, recGain + rampStep)
                                          : std::max (recTarget, recGain - rampStep);
    }

    void advance()
    {
        if (loopLen <= 0) return;
        if (state == State::Stopped && playGain <= 0.0f && ! clearing) return; // parked
        pos += rate * dir;
        while (pos >= (double) loopLen) pos -= (double) loopLen;
        while (pos < 0.0) pos += (double) loopLen;
    }

    void startFirstRecording()
    {
        resetAll();
        state = State::Recording;
        recLen = 0;
        recStopAt = 0;
        recGain = 1.0f; recTarget = 1.0f;
    }

    void closeFirstRecording (State after)
    {
        afterFirst = after;
        if (quantUnit > 1.0 && recLen > 0)
        {
            const double units = std::max (1.0, std::round ((double) recLen / quantUnit));
            const int64_t target = std::min (maxLen, (int64_t) std::llround (units * quantUnit));
            if (target > recLen) { recStopAt = target; return; } // keep recording until the grid
            recLen = target;
        }
        finishFirstRecording();
    }

    void finishFirstRecording()
    {
        if (recLen < fadeLen * 2) { discardAll(); return; }
        loopLen = recLen;
        recStopAt = 0;

        // fade the tail into the seam (head was faded on the way in)
        for (int i = 0; i < fadeLen; ++i)
        {
            const float g = (float) i / (float) fadeLen;
            const size_t j = (size_t) (loopLen - 1 - i);
            layers[0].data[0][j] *= g;
            layers[0].data[1][j] *= g;
        }
        layers[0].start = 0; layers[0].dir = 1; layers[0].written = loopLen;
        numLayers = 1; numStored = 1;
        layerGain[0] = 1.0f;

        requestedLen.store (loopLen, std::memory_order_release);
        loopId.fetch_add (1, std::memory_order_acq_rel);

        pos = dir > 0 ? 0.0 : (double) (loopLen - 1);
        if (afterFirst == State::Stopped) { state = State::Stopped; playGain = 0.0f; playTarget = 0.0f; }
        else                              { state = State::Playing; playGain = 1.0f; playTarget = 1.0f; }
    }

    void startPlayback()
    {
        pos = dir > 0 ? 0.0 : (double) (loopLen - 1);
        playTarget = 1.0f;
        if (playGain > 0.0f) playGain = 0.0f; // restart from the top, tiny fade-in
    }

    void startOverdub()
    {
        state = State::Overdubbing;
        if (numLayers >= kMaxLayers) { state = State::Playing; return; } // full
        if (! extrasReady()) { pendingDub = true; return; }
        beginDubLayer();
    }

    void beginDubLayer()
    {
        if (dubLayer >= 0) finalizeDub();
        if (numLayers >= kMaxLayers) { state = State::Playing; return; }
        const int k = numLayers;
        Layer& L = layers[k];
        L.start = (int64_t) pos;
        L.dir = dir;
        L.written = 0;
        layerGain[k] = 1.0f; // new take is silent until written, so no fade needed
        dubLayer = k;
        lastWriteIdx = -1;
        numLayers = k + 1;
        numStored = numLayers; // a new take kills the redo history above it
        recGain = 0.0f; recTarget = 1.0f;
        state = State::Overdubbing;
    }

    void stopOverdub()
    {
        pendingDub = false;
        recTarget = 0.0f; // layer keeps writing until the fade-out ramp is done
    }

    void finalizeDub()
    {
        dubLayer = -1;
        recGain = 0.0f;
        recTarget = 0.0f;
    }

    void discardAll()
    {
        resetAll();
        loopId.fetch_add (1, std::memory_order_acq_rel);
    }

    void finishClear()
    {
        clearing = false;
        discardAll();
    }

    void resetAll()
    {
        state = State::Empty;
        loopLen = 0; recLen = 0; recStopAt = 0;
        pos = 0.0;
        numLayers = 0; numStored = 0;
        dubLayer = -1; lastWriteIdx = -1;
        recGain = recTarget = 0.0f;
        playGain = playTarget = 1.0f;
        pendingDub = false; clearing = false;
        for (auto& g : layerGain) g = 0.0f;
        for (auto& L : layers) { L.written = 0; L.start = 0; }
    }

    double sr = 44100.0;
    int64_t maxLen = 0;
    int fadeLen = 64;
    float rampStep = 0.01f;

    Layer layers[kMaxLayers];
    float layerGain[kMaxLayers] {};

    State state = State::Empty, afterFirst = State::Playing;
    int64_t loopLen = 0, recLen = 0, recStopAt = 0;
    double pos = 0.0, rate = 1.0, quantUnit = 0.0;
    int dir = 1;
    int numLayers = 0, numStored = 0;
    int dubLayer = -1;
    int64_t lastWriteIdx = -1;
    float recGain = 0.0f, recTarget = 0.0f, playGain = 1.0f, playTarget = 1.0f;
    bool pendingDub = false, clearing = false;

    std::atomic<uint32_t> loopId { 1 }, allocatedFor { 0 };
    std::atomic<int64_t> requestedLen { 0 };
};
} // namespace lt
