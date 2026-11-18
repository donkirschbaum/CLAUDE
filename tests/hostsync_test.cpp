// Host-sync test: tempo = EXT, host plays at 100 BPM; drum notes must land on 16ths.
#include <cstdio>
#include <cmath>
#include "../src/engine.h"
using namespace anu;
struct L : ParamListener { int run_changes = 0; void OnParameterChangedByEngine(uint32_t i, float) override { if (i == P_RUN) ++run_changes; } };
int main() {
  const int sr = 44100, block = 512; const double bpm = 100.0;
  static AnuEngine e; L l; e.Init(sr, &l);
  e.SetParameter(P_TEMPO, 39); e.SetParameter(P_HH_DENSITY, 1.0f); e.SetParameter(P_BD_DENSITY, 1.0f);
  float a[block], b[block]; long frame = 0; int notes = 0; double max_err = 0;
  const double samples_per_16th = sr * 60.0 / bpm / 4.0;
  for (int i = 0; i < 400; ++i) {
    bool playing = i >= 10 && i < 300;
    double ppq = playing ? (frame - 10 * block) / (sr * 60.0 / bpm) : 0.0;
    e.BeginBlock(block, true, playing, ppq, bpm);
    e.Render(a, b, 0, block); e.EndBlock();
    for (int k = 0; k < e.num_out_events(); ++k) {
      const OutMidi& m = e.out_event(k);
      if ((m.data[0] & 0xf0) == 0x90 && (m.data[0] & 15) == 9) {
        double pos = (frame - 10 * block + m.frame) / samples_per_16th;
        double err = std::fabs(pos - std::round(pos)) * samples_per_16th;
        if (err > max_err) max_err = err; ++notes;
      }
    }
    frame += block;
  }
  printf("drum notes=%d max timing error=%.2f samples, run param changes=%d\n", notes, max_err, l.run_changes);
  return (notes > 0 && max_err < 32) ? 0 : 1;
}
