// Offline render test: plays a bassline + arp + drums into a WAV file.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include "../src/engine.h"

using namespace anu;

struct L : ParamListener { int n = 0; void OnParameterChangedByEngine(uint32_t, float) override { ++n; } };

static void WriteWav(const char* path, const std::vector<float>& d, int sr) {
  FILE* f = fopen(path, "wb");
  uint32_t n = d.size(); uint32_t bytes = n * 2;
  auto w32=[&](uint32_t v){fwrite(&v,4,1,f);}; auto w16=[&](uint16_t v){fwrite(&v,2,1,f);};
  fwrite("RIFF",1,4,f); w32(36+bytes); fwrite("WAVEfmt ",1,8,f); w32(16); w16(1); w16(1); w32(sr); w32(sr*2); w16(2); w16(16);
  fwrite("data",1,4,f); w32(bytes);
  for (float x : d) { float c = x > 1 ? 1 : (x < -1 ? -1 : x); int16_t s = (int16_t)lrintf(c*32767); w16((uint16_t)s); }
  fclose(f);
}

int main(int argc, char** argv) {
  const int sr = 48000, block = 256;
  static AnuEngine e; L l;
  e.Init(sr, &l);
  e.SetParameter(P_CUTOFF, 0.35f); e.SetParameter(P_RESONANCE, 0.6f);
  e.SetParameter(P_VCF_ENV, 0.7f); e.SetParameter(P_DECAY, 0.35f); e.SetParameter(P_SUSTAIN, 0.2f);
  e.SetParameter(P_SUB_LEVEL, 0.5f); e.SetParameter(P_MIX, 0.3f);
  e.SetParameter(P_BD_DENSITY, 0.8f); e.SetParameter(P_SD_DENSITY, 0.6f); e.SetParameter(P_HH_DENSITY, 0.7f);
  e.SetParameter(P_DRUMS_X, 0.3f); e.SetParameter(P_DRUMS_Y, 0.6f);
  e.SetParameter(P_ARP_MODE, 2); e.SetParameter(P_ACIDITY, 8); e.SetParameter(P_TEMPO, 128);
  const char* mode = argc > 2 ? argv[2] : "all";
  if (!strcmp(mode, "synth")) e.SetParameter(P_DRUM_LEVEL, 0);
  if (!strcmp(mode, "drums")) e.SetParameter(P_VOLUME, 0);
  std::vector<float> out; float L_[block], R_[block];
  int total = sr * 8; int drumNotes = 0, seqNotes = 0; float peak = 0; bool bad = false;
  for (int pos = 0; pos < total; pos += block) {
    e.BeginBlock(block, false, false, 0, 120);
    if (pos == 0) e.SetParameter(P_RUN, 1);
    if (pos == block * 4) { uint8_t m[3] = {0x90, 36, 100}; e.HandleMidi(m, 3, 0); uint8_t m2[3] = {0x90, 43, 90}; e.HandleMidi(m2, 3, 10); }
    if (pos >= sr * 4 && pos < sr * 4 + block) { e.SetParameter(P_FUZZ, 1); e.SetParameter(P_SYNC, 1); e.SetParameter(P_FM, 0.4f); }
    e.Render(L_, R_, 0, block);
    e.EndBlock();
    for (int i = 0; i < e.num_out_events(); ++i) { auto& ev = e.out_event(i); if ((ev.data[0] & 0xf0) == 0x90) { if ((ev.data[0] & 15) == 9) ++drumNotes; else ++seqNotes; } }
    for (int i = 0; i < block; ++i) { if (!std::isfinite(L_[i])) bad = true; peak = std::fmax(peak, std::fabs(L_[i])); out.push_back(L_[i]); }
  }
  WriteWav(argc > 1 ? argv[1] : "test.wav", out, sr);
  printf("peak=%.3f finite=%d drumNotesOut=%d seqNotesOut=%d paramCallbacks=%d\n", peak, !bad, drumNotes, seqNotes, l.n);
  return bad ? 1 : 0;
}
