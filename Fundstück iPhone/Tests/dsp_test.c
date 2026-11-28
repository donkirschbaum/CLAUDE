// Host-side test for the DSP core:  make -C Tests  (or see Makefile)
#include "../Shared/DSP/fs_dsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SR 48000.0
static uint32_t rs = 1234;
static float rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return (rs >> 8) / 16777216.0f * 2 - 1; }

// "Recorded" sounds: things a person might make with mouth, table, keys …
static void add_boom(float* x, uint32_t at) {            // low "boom" with the mouth
    double ph = 0;
    for (uint32_t i = 0; i < (uint32_t)(0.35 * SR); i++) {
        float t = i / SR; ph += 2 * M_PI * (70 + 90 * expf(-t / 0.04f)) / SR;
        x[at + i] += 0.7f * sinf(ph) * expf(-t / 0.12f) + 0.02f * rnd() * expf(-t / 0.01f);
    }
}
static void add_tsk(float* x, uint32_t at, float len) {   // "ts" hiss
    float prev = 0;
    for (uint32_t i = 0; i < (uint32_t)(len * SR); i++) {
        float t = i / SR; float n = rnd(); float hp = n - prev; prev = n;
        x[at + i] += 0.35f * hp * expf(-t / (len * 0.3f));
    }
}
static void add_pff(float* x, uint32_t at) {              // "pff/ka" snare-ish
    double ph = 0; float lp = 0;
    for (uint32_t i = 0; i < (uint32_t)(0.25 * SR); i++) {
        float t = i / SR; float n = rnd(); lp += 0.35f * (n - lp);
        ph += 2 * M_PI * 210 / SR;
        x[at + i] += (0.5f * lp + 0.25f * n) * expf(-t / 0.07f) + 0.25f * sinf(ph) * expf(-t / 0.03f);
    }
}
static void add_knock(float* x, uint32_t at, float f) {   // knocking on a table
    double ph = 0;
    for (uint32_t i = 0; i < (uint32_t)(0.3 * SR); i++) {
        float t = i / SR; ph += 2 * M_PI * f / SR;
        x[at + i] += 0.5f * sinf(ph) * expf(-t / 0.08f) + 0.1f * rnd() * expf(-t / 0.004f);
    }
}

int main(int argc, char** argv) {
    const char* outDir = argc > 1 ? argv[1] : ".";
    uint32_t n = (uint32_t)(SR * 9);
    float* x = calloc(n, sizeof(float));
    for (uint32_t i = 0; i < n; i++) x[i] = 0.002f * rnd(); // room noise

    float when[] = { 0.3f, 1.1f, 1.8f, 2.7f, 3.5f, 4.6f, 5.5f, 6.4f, 7.3f };
    add_boom(x, (uint32_t)(when[0] * SR));
    add_tsk(x, (uint32_t)(when[1] * SR), 0.06f);
    add_pff(x, (uint32_t)(when[2] * SR));
    add_knock(x, (uint32_t)(when[3] * SR), 160);
    add_tsk(x, (uint32_t)(when[4] * SR), 0.5f);
    add_knock(x, (uint32_t)(when[5] * SR), 420);
    add_boom(x, (uint32_t)(when[6] * SR));
    add_pff(x, (uint32_t)(when[7] * SR));
    add_tsk(x, (uint32_t)(when[8] * SR), 0.05f);

    int fails = 0;
    fs_slice sl[64];
    int ns = fs_detect_slices(x, n, SR, 0.5f, sl, 64);
    printf("detected %d slices (expected 9)\n", ns);
    for (int i = 0; i < ns; i++)
        printf("  %5.2fs-%5.2fs peak %.2f centroid %6.0f low %.2f high %.2f flat %.2f decay %4.0fms\n",
               sl[i].start / SR, sl[i].end / SR, sl[i].peak, sl[i].centroid, sl[i].lowRatio, sl[i].highRatio,
               sl[i].flatness, sl[i].decayMs);
    if (ns != 9) { printf("FAIL slice count\n"); fails++; }
    for (int i = 0; i < ns && i < 9; i++)
        if (fabs(sl[i].start / SR - when[i]) > 0.01) { printf("FAIL onset %d off by %.3fs\n", i, sl[i].start / SR - when[i]); fails++; }

    fs_kit* kit = fs_build_kit(x, n, SR, 0.5f, 0.7f, FS_STYLE_BOOMBAP, 0);
    if (!kit) { printf("FAIL build\n"); return 1; }
    for (int p = 0; p < FS_NUM_PADS; p++) {
        const float* d = fs_kit_pad_data(kit, p);
        uint32_t len = fs_kit_pad_length(kit, p);
        float pk = 0; int bad = 0;
        for (uint32_t i = 0; i < len; i++) { if (!isfinite(d[i])) bad = 1; if (fabsf(d[i]) > pk) pk = fabsf(d[i]); }
        printf("pad %d %-8s %4.0f ms peak %.2f\n", p, fs_kit_pad_name(kit, p), len * 1000.0 / SR, pk);
        if (bad || pk < 0.1f || pk > 1.0f) { printf("FAIL pad %d\n", p); fails++; }
        char path[512]; snprintf(path, sizeof path, "%s/pad%d_%s.wav", outDir, p, fs_kit_pad_name(kit, p));
        fs_kit_write_pad_wav(kit, p, path);
    }

    // serialization roundtrip
    size_t sz = fs_kit_serialized_size(kit);
    uint8_t* buf = malloc(sz);
    size_t w = fs_kit_serialize(kit, buf, sz);
    fs_kit* k2 = fs_kit_deserialize(buf, w);
    if (w != sz || !k2 || fs_kit_pad_length(k2, 0) != fs_kit_pad_length(kit, 0) ||
        memcmp(fs_kit_pad_data(k2, 3), fs_kit_pad_data(kit, 3), 4 * fs_kit_pad_length(kit, 3)) != 0) { printf("FAIL serialize\n"); fails++; }
    if (fs_kit_deserialize(buf, w - 10) != NULL) { printf("FAIL truncated accepted\n"); fails++; }
    free(buf);

    // engine: 4 bars internal clock at 44.1k output (kit is 48k → resampling)
    fs_engine* e = fs_engine_create(44100);
    fs_engine_load_kit(e, k2, 0);
    fs_engine_set_param(e, FS_PARAM_PLAY, 1);
    fs_engine_set_param(e, FS_PARAM_SWING, 0.12f);
    double tempo = fs_engine_get_param(e, FS_PARAM_TEMPO);
    uint32_t block = 256, total = ((uint32_t)(44100 * 60.0 / tempo * 16) / block) * block;
    float* L = calloc(total + block, 4); float* R = calloc(total + block, 4);
    fs_transport tr = {0};
    uint32_t kicks0 = fs_engine_trigger_count(e, 0);
    for (uint32_t pos = 0; pos < total; pos += block) {
        if (pos == 172 * block) { fs_engine_trigger(e, 6, 120); fs_engine_queue_midi(e, 0x90, 38, 100, 17); }
        fs_engine_render(e, L + pos, R + pos, block, &tr);
    }
    float pk = 0; int bad = 0;
    for (uint32_t i = 0; i < total; i++) { if (!isfinite(L[i]) || !isfinite(R[i])) bad = 1; pk = fmaxf(pk, fmaxf(fabsf(L[i]), fabsf(R[i]))); }
    int kickSteps = 0; for (int s = 0; s < 16; s++) if (fs_engine_get_step(e, 0, s)) kickSteps++;
    uint32_t kicks = fs_engine_trigger_count(e, 0) - kicks0;
    printf("groove render: peak %.2f, kick hits %u (pattern %d x 4 bars), tom-hi taps %u\n",
           pk, kicks, kickSteps, fs_engine_trigger_count(e, 6));
    if (bad || pk < 0.2f || pk > 1.5f) { printf("FAIL render level\n"); fails++; }
    if ((int)kicks != kickSteps * 4) { printf("FAIL sequencer count\n"); fails++; }

    // host sync: host at 120 BPM starting at beat 8, should play the same count per bar
    fs_engine_set_param(e, FS_PARAM_PLAY, 0); fs_engine_render(e, L, R, block, &tr);
    fs_engine_set_param(e, FS_PARAM_PLAY, 1);
    fs_transport ht = { 1, 1, 120.0, 8.0 };
    uint32_t c0 = fs_engine_trigger_count(e, 0);
    uint32_t bar = (uint32_t)(44100 * 2.0);
    for (uint32_t pos = 0; pos + block <= bar; pos += block) {
        ht.beatPosition = 8.0 + pos / (44100 * 0.5);
        fs_engine_render(e, L, R, block, &ht);
    }
    printf("host sync: %u kicks in one bar\n", fs_engine_trigger_count(e, 0) - c0);
    if ((int)(fs_engine_trigger_count(e, 0) - c0) != kickSteps) { printf("FAIL host sync\n"); fails++; }

    // write a demo groove with the built kit
    fs_engine_set_param(e, FS_PARAM_FOLLOW_HOST, 0);
    fs_engine_set_param(e, FS_PARAM_PLAY, 0); fs_engine_render(e, L, R, block, &tr);
    fs_engine_set_param(e, FS_PARAM_PLAY, 1);
    for (uint32_t pos = 0; pos < total; pos += block) fs_engine_render(e, L + pos, R + pos, block, &tr);
    float* mono = malloc(4 * total);
    for (uint32_t i = 0; i < total; i++) mono[i] = 0.5f * (L[i] + R[i]);
    char path[512];
    snprintf(path, sizeof path, "%s/groove_from_recording.wav", outDir); fs_write_wav(path, mono, total, 44100);
    snprintf(path, sizeof path, "%s/source_recording.wav", outDir); fs_write_wav(path, x, n, SR);

    // demo kit
    fs_kit* demo = fs_kit_create_demo(44100, 3);
    for (int p = 0; p < FS_NUM_PADS; p++) if (fs_kit_pad_length(demo, p) < 1000) { printf("FAIL demo pad %d\n", p); fails++; }
    fs_engine_load_kit(e, demo, 0);
    fs_kit* snap = fs_engine_snapshot(e);
    if (!snap || fs_kit_pad_length(snap, 0) != fs_kit_pad_length(demo, 0)) { printf("FAIL snapshot\n"); fails++; }
    fs_kit_destroy(snap);

    // silence must not crash and returns NULL
    float* sil = calloc(48000, 4);
    if (fs_build_kit(sil, 48000, SR, 0.5f, 0.5f, 0, 0) != NULL) { printf("FAIL silence\n"); fails++; }

    fs_engine_destroy(e);
    fs_kit_destroy(kit);
    free(x); free(L); free(R); free(mono); free(sil);
    printf(fails ? "\n%d FAILURES\n" : "\nALL TESTS PASSED\n", fails);
    return fails ? 1 : 0;
}
