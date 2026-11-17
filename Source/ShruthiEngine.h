// Shruthi XT emulation engine (C++ port of the web emulation).
// Modulation, envelope, LFO, sequencer and arpeggiator logic follow the
// Shruthi-1 firmware (c) Emilie Gillet / Mutable Instruments, GPL v3.0.
// This file is part of a derivative work licensed under the GNU GPL v3.0.
#pragma once
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <functional>

namespace shruthi {

// Scalar patch parameters. The order and names match the web UI keys.
#define SHRUTHI_PARAMS(X) \
 X(o1s,0,34,1) X(o1p,0,127,0) X(o1r,-24,24,0) X(op,0,13,0) \
 X(o2s,0,34,2) X(o2p,0,127,16) X(o2r,-24,24,-12) X(o2d,0,127,12) \
 X(bal,0,63,32) X(subl,0,63,0) X(nse,0,63,0) X(sub,0,10,0) \
 X(cut,0,127,100) X(res,0,63,20) X(fenv,-63,63,0) X(flfo,-63,63,0) \
 X(e1a,0,127,0) X(e1d,0,127,50) X(e1s,0,127,20) X(e1r,0,127,60) \
 X(e2a,0,127,0) X(e2d,0,127,40) X(e2s,0,127,80) X(e2r,0,127,40) \
 X(l1w,0,20,0) X(l1r,0,143,96) X(l1a,0,127,0) X(l1m,0,3,0) \
 X(l2w,0,20,0) X(l2r,0,143,80) X(l2a,0,127,0) X(l2m,0,3,0) \
 X(cut2,0,127,0) X(res2,0,63,0) X(fm1,0,14,0) X(fm2,0,15,0) \
 X(q1a,0,31,0) X(q1b,0,31,0) X(q1o,0,9,0) X(q2a,0,31,0) X(q2b,0,31,0) X(q2o,0,9,0) \
 X(port,0,63,0) X(lgt,0,1,0) X(mode,0,2,0) X(bpm,40,240,120) X(grv,0,5,0) X(gam,0,127,0) \
 X(adir,0,4,0) X(arng,1,4,1) X(apat,0,15,0) X(div,0,11,7) X(len,1,16,16)

enum ParamId {
#define X(n,a,b,d) k_##n,
 SHRUTHI_PARAMS(X)
#undef X
 kNumParams
};

struct ParamInfo { const char* id; int min, max, def; };
extern const ParamInfo kParamInfo[kNumParams];

struct Step { int g = 1, n = 48, v = 5, c = 0; };

struct Patch {
  int v[kNumParams];
  int mtx[12][3];
  Step steps[16];
  std::string name = "init";
  Patch();
  int operator[](int i) const { return v[i]; }
};

struct Sys { int brd = 0, oct = 0, raga = 0, tune = 0, lofi = 1; };

class Engine {
public:
  explicit Engine(double sampleRate);
  void setSampleRate(double sr);
  void setPatch(const Patch& p) { P = p; hasPatch = true; }
  void setSys(const Sys& s) { G = s; }
  void setVolume(float v) { vol = v; }
  void setHostTempo(double bpm) { hostBpm = bpm; }
  void noteOn(int n, int v);
  void noteOff(int n);
  void setJam(bool on);
  void setBend(float v);      // -1..1
  void setModWheel(float v);  // 0..1
  void setAftertouch(float v);
  void setCC(int i, float v); // i 0..3
  void panic();
  void process(float* L, float* R, int n);
  int currentStep() const { return lastStep; }
  bool isJam() const { return jam; }

private:
  struct Key { int n, v; bool phys; };
  struct Env { int st = 4, a = 0, b = 0; int ph = 0, pinc = 0; int val = 0; int inc[5] = {0,0,0,0,0}; int tgt[5] = {255,0,0,0,0}; };
  struct Lfo { int ph = 0, inc = 0, intensity = 16383, iinc = 0, sh = 128, os = 0; bool run = true, cyc = false; };
  struct Osc { double ph = 0; bool wrap = false; double p[4] = {0,.25,.5,.75}; double m = 0, n = 0, sh = 0; int c = 0; double lf = 0; };

  Patch P; Sys G; bool hasPatch = false;
  double sr, DSR = 39062.5, CR = 39062.5 / 40.0, dAcc = 1, dInc, cAcc = 1, cInc;
  float vol = 0.8f; double hostBpm = 0;
  int lastStep = -1;
  // part
  std::vector<Key> pressed; bool jam = false, relNext = false; int lastNote = 60;
  std::vector<Key> lastChord;
  bool running = false; int stp = 255, presc = 0, gateCnt = 0; std::vector<int> gen;
  int arpNote = 0, arpOct = 0, arpDir = 1, trans = 0, blank = 0;
  int lfoStep[2] = {0,0}; double clkPh = 0; int swing = 0;
  struct MonoNote { int n, v; }; std::vector<MonoNote> mono;
  // voice
  int src[32] = {0}; int dst[27] = {0};
  Env env[2]; int noAuto[2] = {0,0}, trigSt[2] = {0,0};
  Lfo lfo[2];
  int gate = 0, pv = 60*128, pt = 60*128, pinc = 0, bass = 0, trig = 0, volB = 255;
  double inc[2] = {.005,.005}; int prm[2] = {0,0}; int g1 = 255, g2 = 0, subG = 0, noiseG = 0; double fmR[2] = {1,1};
  int cutB = 128, resB = 0, cv1 = 0, cv2 = 0, vcaB = 0; double cutS = 128; bool osc2Off = false;
  Osc o[2]; double subPh = 0; int trc = 0, trRng = 0, trDec = 0, trGain = 0;
  int noise = 1, crushC = 0, crushV = 128, dOut = 128;
  struct F1 { double y1=0,y2=0,y3=0,y4=0,ic1=0,ic2=0,lp=0,bp=0; } f1; struct F2 { double ic1=0, ic2=0; } f2;
  struct FC { double g=.1,k=0,a1=1,a2=0,a3=0,k1=2,b1=1,b2=0,b3=0,k2=2,F=.1,q=2,drv=1; } fc;
  double dcx = 0, dcy = 0, vcaF = 0, vcaC;
  double envExpo[257]; uint8_t dist[256];
  uint32_t rng = 0x12345678u;

  int rnd8() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (int)(rng >> 24); }
  double rndf() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (rng >> 8) / 16777216.0; }

  void start(); void stop(); int sdur() const; void clock(); void nextStep(); void clockArp(); void clockSeq(); void stopGen();
  int tune(int n) const; void iOn(int n, int v); void iOff(int n);
  void vOn(int pitch, int vel, int port, bool trig); void vOff();
  void eTrig(int i, int st); int eRender(int i); void eUpdate(int i, int a, int d, int s, int r);
  int lfoRender(int i); void tick(); void setFilter(); double filt(double x);
  double osc(Osc& o, int sh, double dt, int prm, int i); int trGen(int sb); int digital();
  static double blep(double t, double dt);
};

} // namespace shruthi
