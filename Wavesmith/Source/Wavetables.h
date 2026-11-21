#pragma once

#include <vector>
#include <cmath>

namespace ws
{

// A bank of band-limited, morphable single-cycle wavetables.
// Every table has `numFrames` frames; every frame is stored at `numLevels`
// mip levels (each level halves the number of harmonics) so that high notes
// do not alias.
class WavetableBank
{
public:
    static constexpr int tableSize = 1024;           // samples per cycle
    static constexpr int numFrames = 32;             // morph frames per table
    static constexpr int numLevels = 10;             // 511, 256, 128 ... 1 harmonics
    static constexpr int numTables = 16;
    static constexpr int stride    = tableSize + 1;  // +1 guard sample for interpolation

    static const WavetableBank& get();               // lazily built, shared

    const float* frame (int table, int frameIdx, int level) const noexcept
    {
        return data.data() + ((size_t) ((table * numFrames + frameIdx) * numLevels + level)) * stride;
    }

    // Pick the mip level for a phase increment given in cycles per sample
    static int levelForIncrement (float inc) noexcept
    {
        if (inc <= 0.0f) return 0;
        const float maxHarm = 0.5f / inc;           // harmonics that fit below Nyquist
        if (maxHarm >= 512.0f) return 0;
        int level = (int) std::ceil (std::log2 (512.0f / maxHarm));
        return level < 0 ? 0 : (level >= numLevels ? numLevels - 1 : level);
    }

    // Read with linear interpolation between frames and samples
    inline float read (int table, float position, int level, float phase) const noexcept
    {
        const float fpos = position * (float) (numFrames - 1);
        int f0 = (int) fpos;
        if (f0 >= numFrames - 1) f0 = numFrames - 2;
        if (f0 < 0) f0 = 0;
        const float fw = fpos - (float) f0;

        const float idx = phase * (float) tableSize;
        int i0 = (int) idx;
        if (i0 >= tableSize) i0 = tableSize - 1;
        const float sw = idx - (float) i0;

        const float* a = frame (table, f0, level);
        const float* b = a + (size_t) numLevels * stride;    // next frame, same level
        const float va = a[i0] + sw * (a[i0 + 1] - a[i0]);
        const float vb = b[i0] + sw * (b[i0 + 1] - b[i0]);
        return va + fw * (vb - va);
    }

private:
    WavetableBank();
    std::vector<float> data;
};

} // namespace ws
