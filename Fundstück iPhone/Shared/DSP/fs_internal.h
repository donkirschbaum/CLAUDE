#ifndef FS_INTERNAL_H
#define FS_INTERNAL_H

#include "fs_dsp.h"

typedef struct {
    float*   data;
    uint32_t length;
    int32_t  role;
    int32_t  choke;   // 0 = none
    char     name[FS_NAME_LEN];
} fs_pad;

struct fs_kit {
    double  sampleRate;
    fs_pad  pads[FS_NUM_PADS];
    float   params[FS_NUM_PARAMS];
    uint8_t steps[FS_NUM_PADS][FS_NUM_STEPS];
};

void  fs_default_params(float* p);
float fs_clamp_param(int32_t id, float v);

static inline uint32_t fs_rand(uint32_t* s) {
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return x;
}
static inline float fs_randf(uint32_t* s) { return (fs_rand(s) >> 8) * (1.0f / 16777216.0f); }

/// Turns a raw slice into a finished drum sound for the given role (malloc'd result).
float* fs_process_for_role(const float* src, uint32_t len, double sr, int32_t role,
                           float character, const fs_slice* features, uint32_t* seed,
                           uint32_t* outLen);
#endif
