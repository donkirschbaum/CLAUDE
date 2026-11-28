#include "fs_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

static const char* kRoleNames[FS_NUM_PADS] = {
    "Kick", "Snare", "Clap", "Hat", "Open Hat", "Tom Lo", "Tom Hi", "Perc"
};

const char* fs_role_name(int32_t role) {
    return (role >= 0 && role < FS_NUM_PADS) ? kRoleNames[role] : "Pad";
}

void fs_default_params(float* p) {
    memset(p, 0, sizeof(float) * FS_NUM_PARAMS);
    p[FS_PARAM_MASTER] = 0.9f;
    p[FS_PARAM_DRIVE] = 0.2f;
    p[FS_PARAM_SWING] = 0.0f;
    p[FS_PARAM_TEMPO] = 96.0f;
    p[FS_PARAM_PLAY] = 0.0f;
    p[FS_PARAM_FOLLOW_HOST] = 1.0f;
    static const float gains[FS_NUM_PADS] = {1.0f, 0.9f, 0.8f, 0.6f, 0.55f, 0.85f, 0.8f, 0.7f};
    static const float pans[FS_NUM_PADS]  = {0.0f, 0.0f, 0.1f, 0.25f, 0.3f, -0.3f, 0.3f, -0.2f};
    for (int i = 0; i < FS_NUM_PADS; i++) {
        p[FS_PARAM_PAD_BASE + i * 4 + FS_PAD_GAIN] = gains[i];
        p[FS_PARAM_PAD_BASE + i * 4 + FS_PAD_TUNE] = 0.0f;
        p[FS_PARAM_PAD_BASE + i * 4 + FS_PAD_DECAY] = 1.0f;
        p[FS_PARAM_PAD_BASE + i * 4 + FS_PAD_PAN] = pans[i];
    }
}

float fs_clamp_param(int32_t id, float v) {
    if (v != v) v = 0.0f; // NaN
    #define CL(lo, hi) return v < (lo) ? (lo) : (v > (hi) ? (hi) : v)
    switch (id) {
        case FS_PARAM_MASTER: CL(0.0f, 1.5f);
        case FS_PARAM_DRIVE: CL(0.0f, 1.0f);
        case FS_PARAM_SWING: CL(0.0f, 0.5f);
        case FS_PARAM_TEMPO: CL(40.0f, 240.0f);
        case FS_PARAM_PLAY:
        case FS_PARAM_FOLLOW_HOST: return v >= 0.5f ? 1.0f : 0.0f;
        default: break;
    }
    if (id >= FS_PARAM_PAD_BASE && id < FS_PARAM_PAD_BASE + FS_NUM_PADS * 4) {
        switch ((id - FS_PARAM_PAD_BASE) % 4) {
            case FS_PAD_GAIN: CL(0.0f, 1.5f);
            case FS_PAD_TUNE: CL(-24.0f, 24.0f);
            case FS_PAD_DECAY: CL(0.05f, 1.0f);
            case FS_PAD_PAN: CL(-1.0f, 1.0f);
        }
    }
    #undef CL
    return v;
}

fs_kit* fs_kit_create(double sampleRate) {
    fs_kit* k = (fs_kit*)calloc(1, sizeof(fs_kit));
    if (!k) return NULL;
    k->sampleRate = sampleRate > 1000.0 ? sampleRate : 44100.0;
    fs_default_params(k->params);
    for (int i = 0; i < FS_NUM_PADS; i++) {
        k->pads[i].role = i;
        k->pads[i].choke = (i == FS_ROLE_HAT_CLOSED || i == FS_ROLE_HAT_OPEN) ? 1 : 0;
        snprintf(k->pads[i].name, FS_NAME_LEN, "%s", kRoleNames[i]);
    }
    return k;
}

void fs_kit_destroy(fs_kit* k) {
    if (!k) return;
    for (int i = 0; i < FS_NUM_PADS; i++) free(k->pads[i].data);
    free(k);
}

fs_kit* fs_kit_copy(const fs_kit* src) {
    if (!src) return NULL;
    fs_kit* k = (fs_kit*)malloc(sizeof(fs_kit));
    if (!k) return NULL;
    memcpy(k, src, sizeof(fs_kit));
    for (int i = 0; i < FS_NUM_PADS; i++) {
        k->pads[i].data = NULL;
        if (src->pads[i].data && src->pads[i].length) {
            k->pads[i].data = (float*)malloc(sizeof(float) * src->pads[i].length);
            if (k->pads[i].data) memcpy(k->pads[i].data, src->pads[i].data, sizeof(float) * src->pads[i].length);
            else k->pads[i].length = 0;
        } else {
            k->pads[i].length = 0;
        }
    }
    return k;
}

void fs_kit_set_pad(fs_kit* k, int32_t pad, const float* data, uint32_t length, const char* name, int32_t role) {
    if (!k || pad < 0 || pad >= FS_NUM_PADS) return;
    fs_pad* p = &k->pads[pad];
    free(p->data);
    p->data = NULL; p->length = 0;
    if (data && length) {
        p->data = (float*)malloc(sizeof(float) * length);
        if (p->data) { memcpy(p->data, data, sizeof(float) * length); p->length = length; }
    }
    if (name) snprintf(p->name, FS_NAME_LEN, "%s", name);
    p->role = role;
    p->choke = (role == FS_ROLE_HAT_CLOSED || role == FS_ROLE_HAT_OPEN) ? 1 : 0;
}

uint32_t fs_kit_pad_length(const fs_kit* k, int32_t pad) { return (k && pad >= 0 && pad < FS_NUM_PADS) ? k->pads[pad].length : 0; }
const float* fs_kit_pad_data(const fs_kit* k, int32_t pad) { return (k && pad >= 0 && pad < FS_NUM_PADS) ? k->pads[pad].data : NULL; }
const char* fs_kit_pad_name(const fs_kit* k, int32_t pad) { return (k && pad >= 0 && pad < FS_NUM_PADS) ? k->pads[pad].name : ""; }
double fs_kit_sample_rate(const fs_kit* k) { return k ? k->sampleRate : 44100.0; }

void fs_kit_set_param(fs_kit* k, int32_t id, float v) { if (k && id >= 0 && id < FS_NUM_PARAMS) k->params[id] = fs_clamp_param(id, v); }
float fs_kit_get_param(const fs_kit* k, int32_t id) { return (k && id >= 0 && id < FS_NUM_PARAMS) ? k->params[id] : 0.0f; }
void fs_kit_set_step(fs_kit* k, int32_t pad, int32_t step, uint8_t v) {
    if (k && pad >= 0 && pad < FS_NUM_PADS && step >= 0 && step < FS_NUM_STEPS) k->steps[pad][step] = v > 127 ? 127 : v;
}
uint8_t fs_kit_get_step(const fs_kit* k, int32_t pad, int32_t step) {
    return (k && pad >= 0 && pad < FS_NUM_PADS && step >= 0 && step < FS_NUM_STEPS) ? k->steps[pad][step] : 0;
}

// ---------- Serialization ----------
// "FSK1" | u32 version | f64 sr | u32 nParams | f32*nParams | u8 steps[8*16] | 8 x pad
// pad: i32 role | i32 choke | char name[32] | u32 len | f32*len
#define FS_MAGIC 0x314B5346u /* 'FSK1' little endian */

size_t fs_kit_serialized_size(const fs_kit* k) {
    if (!k) return 0;
    size_t s = 4 + 4 + 8 + 4 + sizeof(float) * FS_NUM_PARAMS + FS_NUM_PADS * FS_NUM_STEPS;
    for (int i = 0; i < FS_NUM_PADS; i++) s += 4 + 4 + FS_NAME_LEN + 4 + sizeof(float) * (size_t)k->pads[i].length;
    return s;
}

#define PUT(ptr, n) do { if (pos + (n) > cap) return 0; memcpy(out + pos, (ptr), (n)); pos += (n); } while (0)
size_t fs_kit_serialize(const fs_kit* k, uint8_t* out, size_t cap) {
    if (!k || !out) return 0;
    size_t pos = 0;
    uint32_t magic = FS_MAGIC, version = 1, np = FS_NUM_PARAMS;
    PUT(&magic, 4); PUT(&version, 4); PUT(&k->sampleRate, 8); PUT(&np, 4);
    PUT(k->params, sizeof(float) * FS_NUM_PARAMS);
    PUT(k->steps, FS_NUM_PADS * FS_NUM_STEPS);
    for (int i = 0; i < FS_NUM_PADS; i++) {
        const fs_pad* p = &k->pads[i];
        PUT(&p->role, 4); PUT(&p->choke, 4); PUT(p->name, FS_NAME_LEN); PUT(&p->length, 4);
        if (p->length) PUT(p->data, sizeof(float) * (size_t)p->length);
    }
    return pos;
}
#undef PUT

#define GET(ptr, n) do { if (pos + (n) > len) goto fail; memcpy((ptr), in + pos, (n)); pos += (n); } while (0)
fs_kit* fs_kit_deserialize(const uint8_t* in, size_t len) {
    if (!in || len < 24) return NULL;
    size_t pos = 0;
    uint32_t magic = 0, version = 0, np = 0;
    double sr = 0;
    fs_kit* k = NULL;
    GET(&magic, 4); GET(&version, 4);
    if (magic != FS_MAGIC || version != 1) return NULL;
    GET(&sr, 8); GET(&np, 4);
    k = fs_kit_create(sr);
    if (!k) return NULL;
    for (uint32_t i = 0; i < np; i++) {
        float v; GET(&v, 4);
        if (i < FS_NUM_PARAMS) k->params[i] = fs_clamp_param((int32_t)i, v);
    }
    GET(k->steps, FS_NUM_PADS * FS_NUM_STEPS);
    for (int i = 0; i < FS_NUM_PADS; i++) {
        fs_pad* p = &k->pads[i];
        uint32_t l = 0;
        GET(&p->role, 4); GET(&p->choke, 4); GET(p->name, FS_NAME_LEN); GET(&l, 4);
        p->name[FS_NAME_LEN - 1] = 0;
        if (l > 48000u * 60u) goto fail;
        if (l) {
            if (pos + sizeof(float) * (size_t)l > len) goto fail;
            p->data = (float*)malloc(sizeof(float) * l);
            if (!p->data) goto fail;
            memcpy(p->data, in + pos, sizeof(float) * l);
            pos += sizeof(float) * l;
            p->length = l;
        }
    }
    return k;
fail:
    fs_kit_destroy(k);
    return NULL;
}
#undef GET

// ---------- WAV ----------
int32_t fs_write_wav(const char* path, const float* data, uint32_t length, double sampleRate) {
    if (!path || (!data && length)) return 0;
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    uint32_t sr = (uint32_t)(sampleRate + 0.5);
    uint32_t dataBytes = length * 3;
    uint32_t riffSize = 36 + dataBytes;
    uint16_t fmtTag = 1, ch = 1, bits = 24, align = 3;
    uint32_t fmtSize = 16, byteRate = sr * 3;
    fwrite("RIFF", 1, 4, f); fwrite(&riffSize, 4, 1, f); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); fwrite(&fmtSize, 4, 1, f); fwrite(&fmtTag, 2, 1, f); fwrite(&ch, 2, 1, f);
    fwrite(&sr, 4, 1, f); fwrite(&byteRate, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dataBytes, 4, 1, f);
    for (uint32_t i = 0; i < length; i++) {
        float v = data[i];
        if (v > 1.0f) v = 1.0f; else if (v < -1.0f) v = -1.0f;
        int32_t s = (int32_t)lrintf(v * 8388607.0f);
        uint8_t b[3] = { (uint8_t)(s & 0xFF), (uint8_t)((s >> 8) & 0xFF), (uint8_t)((s >> 16) & 0xFF) };
        fwrite(b, 1, 3, f);
    }
    fclose(f);
    return 1;
}

int32_t fs_kit_write_pad_wav(const fs_kit* k, int32_t pad, const char* path) {
    if (!k || pad < 0 || pad >= FS_NUM_PADS) return 0;
    return fs_write_wav(path, k->pads[pad].data, k->pads[pad].length, k->sampleRate);
}
