// Shruthi XT emulation engine – see ShruthiEngine.h. GPL v3.0.
#include "ShruthiEngine.h"
#include "ShruthiData.h"
#include <algorithm>

namespace shruthi {
using namespace shruthi_data;

const ParamInfo kParamInfo[kNumParams] = {
#define X(n,a,b,d) { #n, a, b, d },
 SHRUTHI_PARAMS(X)
#undef X
};

static const int kInitMtx[12][3] = {{0,6,0},{1,2,0},{1,3,0},{1,8,0},{2,8,0},{21,2,0},{21,3,0},{20,1,63},{21,1,16},{8,7,32},{0,7,0},{0,0,0}};

Patch::Patch() {
  for (int i = 0; i < kNumParams; ++i) v[i] = kParamInfo[i].def;
  for (int i = 0; i < 12; ++i) for (int j = 0; j < 3; ++j) mtx[i][j] = kInitMtx[i][j];
}

static inline int clip14(int v) { return v < 0 ? 0 : v > 16383 ? 16383 : v; }

Engine::Engine(double s) {
  setSampleRate(s);
  lastChord = {{60,100,false},{63,100,false},{67,100,false}};
  src[28] = 4; src[29] = 8; src[30] = 16; src[31] = 32; src[8] = 128; src[9] = 255;
  const double mx = 1 - std::exp(-5.0 * 255 / 256);
  for (int i = 0; i < 257; ++i) { double lin = std::min(i, 255) / 256.0; envExpo[i] = (1 - std::exp(-5 * lin)) / mx * 255; }
  double f[256], mn = 1e9, mxx = -1e9;
  for (int i = 0; i < 256; ++i) { f[i] = std::tanh(6 * (i / 128.0 - 1)) * 128 + 128; mn = std::min(mn, f[i]); mxx = std::max(mxx, f[i]); }
  for (int i = 0; i < 256; ++i) dist[i] = (uint8_t)std::lround((f[i] - mn) / (mxx - mn) * 253 + 1);
}

void Engine::setSampleRate(double s) {
  sr = s; dInc = DSR / sr; cInc = CR / sr; vcaC = 1 - std::exp(-1 / (.0007 * sr));
}

/* ---------- part ---------- */
void Engine::noteOn(int n, int v) {
  if (!hasPatch) return;
  if (relNext) {
    std::vector<Key> keep, rel;
    for (auto& k : pressed) (k.phys ? keep : rel).push_back(k);
    pressed = keep; if (P[k_mode] == 0) for (auto& k : rel) iOff(k.n);
    relNext = false;
  }
  pressed.erase(std::remove_if(pressed.begin(), pressed.end(), [n](const Key& k){ return k.n == n; }), pressed.end());
  pressed.push_back({n, v, true});
  lastNote = n; lastChord.clear(); for (auto& k : pressed) lastChord.push_back({k.n, k.v, false});
  if (!running) start();
  if (P[k_mode] == 0) iOn(n, v);
}

void Engine::noteOff(int n) {
  if (!hasPatch) return;
  auto it = std::find_if(pressed.begin(), pressed.end(), [n](const Key& k){ return k.n == n; });
  if (it == pressed.end()) return;
  if (jam) {
    it->phys = false;
    if (std::none_of(pressed.begin(), pressed.end(), [](const Key& k){ return k.phys; })) relNext = true;
  } else {
    pressed.erase(it);
    if (P[k_mode] == 0) iOff(n);
  }
}

void Engine::setJam(bool on) {
  if (!hasPatch) return;
  jam = on;
  if (on) {
    if (pressed.empty()) {
      std::vector<Key> ns = P[k_mode] == 1 ? lastChord : std::vector<Key>{{lastNote, 100, false}};
      for (auto& k : ns) { pressed.push_back({k.n, k.v, false}); if (P[k_mode] == 0) iOn(k.n, k.v); }
    }
    if (!running) start();
    relNext = std::none_of(pressed.begin(), pressed.end(), [](const Key& k){ return k.phys; });
  } else {
    std::vector<Key> keep, rel;
    for (auto& k : pressed) (k.phys ? keep : rel).push_back(k);
    pressed = keep; if (P[k_mode] == 0) for (auto& k : rel) iOff(k.n);
    relNext = false;
  }
}

void Engine::setBend(float v) { src[8] = std::max(0, std::min(255, (int)std::lround((v + 1) * 127.5))); }
void Engine::setModWheel(float v) { src[6] = (int)std::lround(v * 254); }
void Engine::setAftertouch(float v) { src[7] = (int)std::lround(v * 254); }
void Engine::setCC(int i, float v) { if (i >= 0 && i < 4) src[14 + i] = (int)std::lround(v * 254); }
void Engine::panic() { pressed.clear(); jam = false; relNext = false; stopGen(); mono.clear(); vOff(); running = false; lastStep = -1; }

void Engine::start() {
  lfoStep[0] = lfoStep[1] = 0;
  if (P[k_mode] != 0) { mono.clear(); gen.clear(); vOff(); }
  presc = 0; stp = 255; running = true; trans = 0; blank = 0; clkPh = 1e6;
  if (P[k_adir] == 1) { arpNote = (int)pressed.size() - 1; arpOct = P[k_arng] - 1; arpDir = -1; }
  else { arpNote = 0; arpOct = 0; arpDir = 1; }
}
void Engine::stop() { running = false; if (P[k_mode] != 0) { stopGen(); mono.clear(); vOff(); } lastStep = -1; }
int Engine::sdur() const { static const int d[12] = {96,48,32,24,16,12,8,6,4,3,2,1}; int i = P[k_div]; return (i >= 0 && i < 12) ? d[i] : 6; }

void Engine::clock() {
  if (running) {
    if (pressed.empty()) { if (++blank >= 12) { stop(); return; } } else blank = 0;
    if (presc == 0) { nextStep(); clockArp(); clockSeq(); }
    if (gateCnt == 0 && !gen.empty()) stopGen(); else gateCnt = (gateCnt - 1) & 255;
  }
  const int sd = sdur();
  const double bpm = hostBpm > 0 ? hostBpm : (P[k_bpm] < 40 ? 120 : P[k_bpm]);
  const double hz = bpm * 24 / 60;
  for (int i = 0; i < 2; ++i) {
    const int r = P[i ? k_l2r : k_l1r];
    if (r < 16) {
      const int lim = sd * (r + 1); lfoStep[i] %= lim;
      lfo[i].ph = (int)std::floor(lfoStep[i] * 65536.0 / lim) & 65535;
      lfo[i].inc = std::max(1, (int)std::lround(65536.0 / lim * hz / CR));
    }
    if (++lfoStep[i] >= 1536) lfoStep[i] = 0;
  }
  if (++presc >= sd) presc = 0;
}

void Engine::nextStep() {
  stp = (stp + 1) & 255; if (stp >= P[k_len]) stp = 0;
  if (P[k_mode] == 2 && P.steps[stp & 15].g == 2) gateCnt += sdur();
  lastStep = stp;
}

void Engine::clockArp() {
  const int n = stp & 15; bool has;
  if (P[k_apat] < 15) has = (kArpPatterns[std::min(P[k_apat], kNumArpPatterns - 1)] >> stp) & 1;
  else has = P.steps[n].g == 1;
  src[5] = has ? 255 : 0;
  const int N = (int)pressed.size();
  if (P[k_mode] == 1 && has && N) {
    const int rngA = P[k_arng];
    if (N == 1 && rngA == 1) { arpNote = 0; arpOct = 0; }
    else if (P[k_adir] == 3) { int r = rnd8(); arpOct = (r & 15) % rngA; arpNote = ((r >> 4) & 15) % N; }
    else {
      bool w = true; int g = 0;
      while (w && g++ < 8) {
        if (arpNote >= N || arpNote < 0) { arpOct += arpDir; arpNote = arpDir > 0 ? 0 : N - 1; }
        w = false;
        if (arpOct >= rngA || arpOct < 0) {
          arpOct = arpDir > 0 ? 0 : rngA - 1;
          if (P[k_adir] == 2) { arpDir = -arpDir; arpNote = arpDir > 0 ? 1 : N - 2; arpOct = arpDir > 0 ? 0 : rngA - 1; w = true; }
        }
      }
    }
    stopGen();
    std::vector<Key> list = pressed;
    if (P[k_adir] != 4) std::sort(list.begin(), list.end(), [](const Key& a, const Key& b){ return a.n < b.n; });
    const Key& e = list[std::max(0, std::min(N - 1, arpNote))];
    int note = e.n + 12 * arpOct; while (note > 127) note -= 12;
    gen.push_back(note); iOn(note, e.v & 127);
    arpNote += arpDir; gateCnt = (sdur() >> 1) + 1;
  }
  if (P[k_mode] == 1 && N && P[k_apat] == 15) { int m = stp + 1; if (m >= P[k_len]) m = 0; if (P.steps[m & 15].g == 2) gateCnt += sdur(); }
}

void Engine::clockSeq() {
  const int n = stp & 15;
  src[2] = P.steps[n].c << 4; src[3] = P.steps[n & 7].c << 4; src[4] = P.steps[(n & 7) + 8].c << 4;
  if (P[k_mode] == 2) {
    const Step& st = P.steps[n];
    if (st.g) {
      const int tr = !pressed.empty() ? pressed.back().n - 60 : trans; trans = tr;
      int note = st.n + tr; while (note < 0) note += 12; while (note > 127) note -= 12;
      const int vel = st.v << 4;
      if (st.g != 2) { stopGen(); iOn(note, vel); }
      else { const int last = gen.empty() ? -1 : gen.back(); if (last != note) { iOn(note, vel); stopGen(); } }
      if (std::find(gen.begin(), gen.end(), note) == gen.end()) gen.push_back(note);
      gateCnt = (sdur() >> 1) + 1;
    }
  }
}

void Engine::stopGen() { while (!gen.empty()) { int n = gen.front(); gen.erase(gen.begin()); iOff(n); } }

int Engine::tune(int n) const {
  const int r = G.raga; if (r <= 0 || r > kNumScales) return n * 128;
  const int s = kScales[(r - 1) * 12 + n % 12]; return s == 32767 ? 0 : n * 128 + s;
}

void Engine::iOn(int n, int v) {
  const int before = mono.empty() ? -1 : mono.back().n;
  mono.erase(std::remove_if(mono.begin(), mono.end(), [n](const MonoNote& x){ return x.n == n; }), mono.end());
  mono.push_back({n, v});
  if (before != n) {
    const bool leg = mono.size() > 1;
    vOn(tune(n), v, (!P[k_lgt] || leg) ? P[k_port] : 0, !P[k_lgt] || !leg);
    const int b = mono.front().n; bass = b != n ? tune(b) : 0;
  }
}

void Engine::iOff(int n) {
  if (mono.empty()) return;
  const int before = mono.back().n;
  mono.erase(std::remove_if(mono.begin(), mono.end(), [n](const MonoNote& x){ return x.n == n; }), mono.end());
  if (mono.empty()) { vOff(); bass = 0; return; }
  const MonoNote a = mono.back();
  if (a.n != before) vOn(tune(a.n), a.v, P[k_port], !P[k_lgt]);
  const int b = mono.front().n; bass = b != a.n ? tune(b) : 0;
}

/* ---------- voice ---------- */
void Engine::vOn(int pitch, int vel, int port, bool trg) {
  if (!pitch) return;
  pt = pitch;
  if (trg || pv == 0) {
    for (int i = 0; i < 2; ++i) if (!noAuto[i]) eTrig(i, 0);
    trig = (trig + 1) & 255; gate = 255;
    for (int i = 0; i < 2; ++i) { if (P[i ? k_l2m : k_l1m]) { lfo[i].ph = 0; lfo[i].cyc = true; lfo[i].run = true; } lfo[i].intensity = 0; }
    trc = 255; src[21] = std::min(254, vel << 1); src[22] = rnd8();
    o[0].ph = 0; o[1].ph = 0;
  }
  if (port) { const int d = pt - pv; pinc = (d * (int)kEnvIncrements[port]) >> 16; if (!pinc) pinc = d < 0 ? -1 : 1; }
  else { pv = pt; pinc = 1; }
}
void Engine::vOff() { gate = 0; eTrig(0, 3); eTrig(1, 3); }

void Engine::eTrig(int i, int st) { Env& e = env[i]; if (st == 4) e.val = 0; e.a = e.val >> 8; e.b = e.tgt[st]; e.st = st; e.ph = 0; e.pinc = e.inc[st]; }

int Engine::eRender(int i) {
  Env& e = env[i]; if (!e.pinc && e.st != 2 && e.st != 4) e.pinc = e.inc[e.st]; e.ph += e.pinc;
  if (e.ph > 65535) { e.ph -= 65536; e.val = e.b * 255; eTrig(i, std::min(4, e.st + 1)); }
  if (e.pinc) { const int k = e.ph >> 8, f = e.ph & 255; const double s = envExpo[k] + (envExpo[k + 1] - envExpo[k]) * f / 256.0; e.val = (int)std::lround(e.a * (255 - s) + e.b * s); }
  return e.st == 2 ? e.tgt[1] : std::min(255, e.val >> 8);
}

void Engine::eUpdate(int i, int a, int d, int s, int r) {
  Env& e = env[i]; e.inc[0] = kEnvIncrements[a]; e.inc[1] = kEnvIncrements[d]; e.inc[3] = kEnvIncrements[r]; e.tgt[1] = s << 1; e.tgt[2] = s << 1;
}

int Engine::lfoRender(int i) {
  Lfo& L = lfo[i]; const int w = P[i ? k_l2w : k_l1w], md = P[i ? k_l2m : k_l1m];
  if (L.intensity < 16383) { int v = L.intensity + L.iinc; if ((v >> 8) >= 0x40) v = 16383; L.intensity = v; }
  L.cyc = L.ph < L.inc;
  int v;
  switch (w) {
    case 0: v = (L.ph & 0x8000) ? (L.ph >> 7) & 255 : (~(L.ph >> 7)) & 255; break;
    case 1: v = (L.ph & 0x8000) ? 255 : 0; break;
    case 2: if (L.cyc) L.sh = rnd8(); v = L.sh; break;
    case 3: v = L.ph >> 8; break;
    case 4: v = P.steps[((L.ph >> 8) * P[k_len]) >> 8].c << 4; break;
    default: { int off = w - 5; off = off == 0 ? 3 : off + 16; v = kWaves[off * 129 + (L.ph >> 9)]; }
  }
  L.ph = (L.ph + L.inc) & 65535;
  if (md == 3) {
    if (L.run) { if (L.ph < L.inc) { L.run = false; if (w == 3 || w == 0 || w == 1) v = 255; else if (w == 4) v = 0; L.os = v; } } else v = L.os;
    v = w == 4 ? 128 + (v >> 1) : 255 - (v >> 1);
  }
  return (((v - 128) * (L.intensity >> 6)) >> 8) + 128;
}

void Engine::tick() {
  int* S = src; int* D = dst;
  DSR = G.lofi ? 39062.5 : sr; dInc = DSR / sr;
  const double bpm = hostBpm > 0 ? hostBpm : (P[k_bpm] < 40 ? 120 : P[k_bpm]);
  clkPh += bpm * 24 / 60 / CR;
  const double lim = (128 + swing) / 128.0;
  if (clkPh >= lim) {
    clkPh = clkPh > 100 ? 0 : clkPh - lim;
    const int gi = std::min(5, std::max(0, P[k_grv]));
    swing = ((kGrooves[gi * 16 + (stp & 15)] * P[k_gam]) >> 8) >> 1;
    clock();
  }
  for (int i = 0; i < 2; ++i) S[i] = lfoRender(i);
  S[18] = rnd8(); noise = S[18];
  for (int i = 0; i < 2; ++i) {
    const int r = P[i ? k_l2r : k_l1r];
    lfo[i].iinc = kEnvIncrements[P[i ? k_l2a : k_l1a]] >> 1;
    if (r >= 16) { int rt = r + (D[15 + i] >> 8) - 32; rt = rt < 0 ? 0 : rt > 127 ? 127 : rt; lfo[i].inc = kLfoIncrements[rt]; }
  }
  for (int j = 0; j < 2; ++j) if (lfo[j].cyc && P[j ? k_l2m : k_l1m] == 2) eTrig(j, 0);
  S[19] = eRender(0); S[20] = eRender(1);
  S[23] = std::min(255, std::max(0, pv >> 6)); S[24] = gate; S[25] = dOut;
  for (int i = 0; i < 2; ++i) {
    const int op = P[i ? k_q2o : k_q1o]; if (!op) continue;
    int x = S[P[i ? k_q2a : k_q1a]] & 255, y = S[P[i ? k_q2b : k_q1b]] & 255, r = 0;
    switch (op) {
      case 1: r = (x >> 1) + (y >> 1); break; case 2: r = (x * y) >> 8; break;
      case 3: r = std::max(x, y); break; case 4: r = std::min(x, y); break; case 5: r = x ^ y; break;
      case 6: r = x > y ? 255 : 0; break; case 7: r = x > y ? 0 : 255; break;
      case 8: { int m = 0; while (y >>= 1) m = (m >> 1) | 0x80; r = x & m; } break;
      default: { y = (y >> 2) + 1; r = ((256 - y) * S[26 + i] + y * x) >> 8; }
    }
    S[26 + i] = r;
  }
  D[0] = P[k_cut] * 128; D[2] = P[k_o1p] * 128; D[3] = P[k_o2p] * 128; D[4] = D[5] = D[6] = D[7] = 8192; D[15] = D[16] = 8192; D[17] = D[18] = 0;
  D[11] = P[k_res] << 8; D[8] = P[k_bal] << 8; D[9] = P[k_nse] << 8; D[10] = P[k_subl] << 8; D[14] = 8192;
  D[19] = P[k_e1a] * 128; D[20] = P[k_e1d] * 128; D[21] = P[k_e1s] * 128; D[22] = P[k_e1r] * 128;
  D[23] = P[k_e2a] * 128; D[24] = P[k_e2d] * 128; D[25] = P[k_e2s] * 128; D[26] = P[k_e2r] * 128;
  D[12] = 0; D[13] = 0; if (G.brd >= 1) { D[12] = P[k_cut2] * 128; D[13] = P[k_res2] << 8; }
  noAuto[0] = noAuto[1] = 0; int vca = volB;
  for (int i = 0; i < 12; ++i) {
    int amt = P.mtx[i][2]; if (!amt) continue;
    if (i == 11) { amt = (amt * S[6]) >> 8; if (!amt) continue; }
    const int s = std::min(31, std::max(0, P.mtx[i][0])), d = P.mtx[i][1]; if (d < 0 || d > 26) continue;
    if (d == 17 || d == 18) noAuto[d - 17] = 1;
    int sv = S[s] & 255;
    if (d != 1) { int mod = D[d]; mod += (s <= 1 || s == 8 || s == 23 || s == 25) ? amt * (sv - 128) : amt * sv; D[d] = clip14(mod); }
    else { if (amt < 0) { amt = -amt; sv = 255 - sv; } if (amt != 63) { const int g = amt << 2; sv = (255 * (255 - g) + sv * g) >> 8; } vca = (vca * sv) >> 8; }
  }
  vcaB = vca;
  int cut = D[0];
  if (P[k_op] != 9) cut = clip14(cut + pv - 8192 + (G.brd == 4 ? 2048 : 0));
  cut = clip14(cut + P[k_fenv] * S[19]); cut = clip14(cut + P[k_flfo] * (S[1] - 128));
  if (G.brd == 2 && P[k_fm1] >= 3) D[12] = clip14(D[12] + cut - 8192);
  cutB = cut >> 6; resB = D[11] >> 6; cv1 = D[12] >> 6; cv2 = D[13] >> 6;
  for (int j = 0; j < 2; ++j) { if (D[17 + j] > 6000) { if (!trigSt[j]) eTrig(j, 0); trigSt[j] = 1; } else trigSt[j] = 0; }
  prm[0] = D[2] >> 7; prm[1] = D[3] >> 7;
  const int am = ((D[14] >> 7) - 64) << 1;
  auto cl7 = [](int v){ return v < 0 ? 0 : v > 127 ? 127 : v; };
  eUpdate(0, cl7((D[19] >> 7) - am), D[20] >> 7, D[21] >> 7, D[22] >> 7);
  eUpdate(1, cl7((D[23] >> 7) - am), D[24] >> 7, D[25] >> 7, D[26] >> 7);
  int bp = pv + pinc; if ((pinc > 0) != (bp < pt)) { bp = pt; pinc = 0; } pv = bp;
  bp += (D[6] - 8192) >> 4; bp += (D[7] - 8192) >> 7; bp += G.tune;
  for (int i = 0; i < 2; ++i) {
    int p = bp; if (P[k_op] == 9 && i == 0 && bass) p = p - pv + bass;
    const int sh = P[i ? k_o2s : k_o1s], rg = P[i ? k_o2r : k_o1r];
    p += ((sh != 10 ? rg : 0) + G.oct * 12) * 128; if (i == 1) p += P[k_o2d]; p += (D[4 + i] - 8192) >> 2;
    while (p >= 128 * 128) p -= 12 * 128;
    inc[i] = std::min(.45, 440.0 * std::pow(2.0, (p / 128.0 - 69) / 12) / DSR);
    const int sec = rg + 24, off = sec < 12 ? 0 : sec > 36 ? 24 : sec - 12; fmR[i] = kFmRatios[off] / 256.0;
  }
  osc2Off = P[k_op] == 9 && !bass;
  int gg2 = D[8] >> 6, gg1 = 255 - gg2, mask = 255; const int op = P[k_op];
  static const int m4[4] = {4,1,8,2}, m8[8] = {4,1,8,8,2,4,8,2};
  if (op == 10) mask = (trig & 1) ? 0x0e : 0x0d; else if (op == 11) mask = m4[trig & 3]; else if (op == 12) mask = m8[trig & 7]; else if (op == 13) mask = S[2] >> 4;
  if (mask != 255 && (mask & 3) != 3) { gg2 = (mask & 1) ? 255 : 0; gg1 = (mask & 2) ? 255 : 0; }
  int sg = D[10] >> 7; if (mask != 255) { sg = (mask & 4) ? sg : 0; if ((mask & 3) == 0) sg = std::min(255, sg << 1); }
  int ng = D[9] >> 8; if (mask != 255) { ng = (mask & 8) ? ng : 0; if (mask == 8) ng = std::min(255, ng << 1); }
  g1 = gg1; g2 = gg2; subG = sg; noiseG = ng;
  setFilter();
}

/* ---------- analog filter ---------- */
void Engine::setFilter() {
  cutS += (cutB - cutS) * .5;
  auto fcOf = [this](double b){ return std::min(13.0 * std::pow(2.0, b / 24), sr * .45); };
  const double f = fcOf(cutS), r = resB / 255.0;
  if (G.brd == 2) {
    auto svf = [this](double fr, double k, double& a1, double& a2, double& a3){ const double g = std::tan(M_PI * std::min(fr, sr * .49) / sr); a1 = 1 / (1 + g * (g + k)); a2 = g * a1; a3 = g * a2; };
    fc.k1 = 2 - 2 * r * .985; svf(f, fc.k1, fc.a1, fc.a2, fc.a3);
    fc.k2 = 2 - 2 * (cv2 / 255.0) * .985; svf(fcOf(cv1), fc.k2, fc.b1, fc.b2, fc.b3);
  } else if (G.brd == 4) { fc.F = std::min(1.0, 2 * std::sin(M_PI * f / (4 * sr))); fc.q = 2 - r * 1.97; fc.drv = 1 + cv1 / 255.0 * 5; }
  else { fc.g = 1 - std::exp(-2 * M_PI * f / (2 * sr)); fc.k = r * 4.1; }
}

double Engine::filt(double x) {
  static const double P4[15][5] = {{0,0,0,0,1},{0,0,0,1,0},{0,0,1,0,0},{0,1,0,0,0},{1,-1,0,0,0},{1,-2,1,0,0},{1,-3,3,-1,0},{0,2,-2,0,0},{0,0,4,-8,4},{1,-2,2,0,0},{1,-4,8,-8,4},{0,0,1,-2,1},{0,1,-3,3,-1},{.5,-1,1,0,.5},{.5,-2,4,-4,2.5}};
  if (G.brd == 2) {
    const double v3 = x - f1.ic2, v1 = fc.a1 * f1.ic1 + fc.a2 * v3, v2 = f1.ic2 + fc.a2 * f1.ic1 + fc.a3 * v3;
    f1.ic1 = 2 * v1 - f1.ic1; f1.ic2 = 2 * v2 - f1.ic2;
    const int m1 = P[k_fm1] % 3; const double y1 = m1 == 0 ? v2 : m1 == 1 ? v1 : x - fc.k1 * v1 - v2;
    const bool serial = P[k_fm2] >= 3; const double in = serial ? y1 : x;
    const double w3 = in - f2.ic2, w1 = fc.b1 * f2.ic1 + fc.b2 * w3, w2 = f2.ic2 + fc.b2 * f2.ic1 + fc.b3 * w3;
    f2.ic1 = 2 * w1 - f2.ic1; f2.ic2 = 2 * w2 - f2.ic2;
    const int m2 = P[k_fm2] % 3; const double y2 = m2 == 0 ? w2 : m2 == 1 ? w1 : in - fc.k2 * w1 - w2;
    f1.ic1 = std::tanh(f1.ic1 * .5) * 2; f2.ic1 = std::tanh(f2.ic1 * .5) * 2;
    return serial ? y2 : (y1 + y2) * .6;
  }
  if (G.brd == 4) {
    x = std::tanh(x * fc.drv); double hp = 0;
    for (int j = 0; j < 2; ++j) { f1.lp += fc.F * f1.bp; hp = x - f1.lp - fc.q * f1.bp; f1.bp += fc.F * hp; f1.bp = std::max(-1.3, std::min(1.3, f1.bp)); f1.lp = std::max(-2.0, std::min(2.0, f1.lp)); }
    return (P[k_fm1] % 2) ? f1.bp : f1.lp;
  }
  const double g = fc.g, k = fc.k; double u = 0;
  for (int j = 0; j < 2; ++j) { u = std::tanh(x * (1 + k * .4) - k * f1.y4); f1.y1 += g * (u - f1.y1); f1.y2 += g * (f1.y1 - f1.y2); f1.y3 += g * (f1.y2 - f1.y3); f1.y4 += g * (f1.y3 - f1.y4); }
  if (G.brd == 5) { const double* m = P4[std::min(14, P[k_fm1])]; return (m[0] * u + m[1] * f1.y1 + m[2] * f1.y2 + m[3] * f1.y3 + m[4] * f1.y4) * .8; }
  return f1.y4;
}

/* ---------- digital oscillators ---------- */
double Engine::blep(double t, double dt) { if (t < dt) { t /= dt; return t + t - t * t - 1; } if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; } return 0; }

double Engine::osc(Osc& O, int sh, double dt, int pr, int i) {
  static const double FORM[5][3] = {{730,1090,2440},{530,1840,2480},{270,2290,3010},{570,840,2410},{300,870,2240}};
  double ph = O.ph + dt; O.wrap = false; if (ph >= 1) { ph -= 1; O.wrap = true; } O.ph = ph;
  const double TAU = 6.283185307179586, pf = pr / 127.0; double y = 0;
  switch (sh) {
    case 0: return 128;
    case 1: { double p2 = ph + .5; if (p2 >= 1) p2 -= 1; y = (2 * ph - 1 - blep(ph, dt) + pf * (2 * p2 - 1 - blep(p2, dt))) / (1 + pf * .5); } break;
    case 2: { const double pw = .5 - pf * .46; double q = ph - pw; if (q < 0) q += 1; y = (ph < pw ? 1 : -1) + blep(ph, dt) - blep(q, dt); } break;
    case 3: { const double tri = ph < .5 ? 4 * ph - 1 : 3 - 4 * ph, c = -1 + pf * 1.6, v = tri < c ? c : tri; y = (v - c) / (1 - c) * 2 - 1; } break;
    case 4: { const double d = .5 - pf * .48, q = ph < d ? ph / d * .5 : .5 + (ph - d) / (1 - d) * .5; y = -std::cos(TAU * q); } break;
    case 5: y = (1 - ph) * std::cos(TAU * ph * (1 + pf * 14)) * 1.4; break;
    case 6: { const double w = ph < .5 ? 2 * ph : 2 - 2 * ph; y = w * std::cos(TAU * ph * (1 + pf * 14)) * 1.4; } break;
    case 7: { const double q = ph < .5 ? ph * 2 : ph * 2 - 1; y = (ph < .5 ? 1 : -1) * (1 - q) * std::cos(TAU * q * (1 + pf * 14)) * 1.2; } break;
    case 8: y = std::sin(TAU * ph * (1 + pf * 12)) * (ph < .5 ? 1 : -1); break;
    case 9: { const double d = pf * .03, R[4] = {1, 1 + d, 1 - d * .8, 1 + d * 2.1}; double s = 0; for (int k = 0; k < 4; ++k) { double p = O.p[k] + dt * R[k]; if (p >= 1) p -= 1; O.p[k] = p; s += 2 * p - 1; } y = s * .42; } break;
    case 10: { O.m += dt * fmR[i]; if (O.m >= 1) O.m -= std::floor(O.m); const double m8 = (std::sin(TAU * O.m) + 1) * 127.5; y = std::sin(TAU * (ph + m8 * pr * 2 / 65536.0)); } break;
    case 19: { const int wi = pr >> 4, g = (pr & 15) << 4, w2 = wi < 7 ? wi + 1 : wi; const double pos = ph * 128; const int pi = (int)pos; const double fr = pos - pi;
      const double a = kWaves[wi * 129 + pi] + (kWaves[wi * 129 + pi + 1] - kWaves[wi * 129 + pi]) * fr, b = kWaves[w2 * 129 + pi] + (kWaves[w2 * 129 + pi + 1] - kWaves[w2 * 129 + pi]) * fr;
      return (a * (255 - g) + b * g) / 255; }
    case 20: { const int x = (int)(ph * 256), m = 1 + (pr >> 3); return (x ^ ((x * m) >> 3)) & 255; }
    case 21: { O.c++; const int hold = 1 + (int)(pf * pf * 48); if (O.c >= hold) { O.c = 0; const double q = std::pow(2.0, std::max(1, 7 - (int)(pf * 6))); O.sh = std::round(std::sin(TAU * ph) * q) / q; } y = O.sh; } break;
    case 22: { O.lf += (.15 + pf * 5) / DSR; if (O.lf >= 1) O.lf -= 1; const double lt = O.lf < .5 ? 4 * O.lf - 1 : 3 - 4 * O.lf, pw = .5 + lt * (.08 + pf * .38); double q = ph - pw; if (q < 0) q += 1; y = (ph < pw ? 1 : -1) + blep(ph, dt) - blep(q, dt); } break;
    case 23: { const double w = rndf() * 2 - 1, c = 1 - pf * .96; O.n += (w - O.n) * c; y = O.n * (1 + pf * 2.5); } break;
    case 24: { const double f0 = dt * DSR, v = pf * 4; const int vi = std::min(3, (int)v); const double vf = v - vi, win = (1 - ph) * (1 - ph), Gk[3] = {1, .5, .25}; double s = 0;
      for (int k = 0; k < 3; ++k) { const double c = FORM[vi][k] * (1 - vf) + FORM[vi + 1][k] * vf; s += Gk[k] * std::sin(TAU * ph * c / f0); } y = s * win * 1.1; } break;
    default: {
      const int idx = sh >= 25 ? sh - 25 + 8 : sh - 11; if (idx < 0 || idx > 17) return 128;
      const int b = idx * 18, ns = kWavetables[b], ptr = (pr << 1) * ns, w1 = kWavetables[b + 1 + (ptr >> 8)], w2 = kWavetables[b + 2 + (ptr >> 8)], g = ptr & 255;
      const double pos = ph * 128; const int pi = (int)pos; const double fr = pos - pi; const int i1 = w1 * 129 + pi, i2 = w2 * 129 + pi;
      const double a = kWaves[i1] + (kWaves[i1 + 1] - kWaves[i1]) * fr, c = kWaves[i2] + (kWaves[i2 + 1] - kWaves[i2]) * fr;
      return (a * (255 - g) + c * g) / 255;
    }
  }
  y = y * 127.5 + 127.5; return y < 0 ? 0 : y > 255 ? 255 : y;
}

int Engine::trGen(int sb) {
  int v = 0;
  switch (sb) {
    case 6: trGain = trc; trc--; v = trc < 32 ? 255 : 0; break;
    case 7: trGain = trc; trc--; trRng = (trRng * 73 + trc) & 255; v = trRng; break;
    case 8: trDec = (trDec + 2) & 255; if (trDec >= 16) { trDec = (trDec - 17) & 255; trRng = (trRng * 73 + trc) & 255; if (trDec == 0) { trc--; trGain = (trc & 0x80) ? (~trc) & 255 : trc; } } v = trRng; break;
    case 9: trc--; trGain = trc >= 64 ? 255 : (trc << 2); v = (trc * 57) & 255; break;
    default: trc--; trGain = trc > 0 ? 255 : 0; v = 0;
  }
  return v;
}

int Engine::digital() {
  const int a = (int)std::lround(osc(o[0], P[k_o1s], inc[0], prm[0], 0)) & 255;
  if (P[k_op] == 1 && o[0].wrap) o[1].ph = 0;
  const int b = osc2Off ? 128 : (int)std::lround(osc(o[1], P[k_o2s], inc[1], prm[1], 1)) & 255;
  int x;
  if (!g1 && !g2) x = 128;
  else switch (P[k_op]) {
    case 2: { const int r = ((((a - 128) * (b - 128)) >> 8) + 128) & 255; x = (a * g1 + r * g2) >> 8; } break;
    case 3: x = (a ^ b ^ g2) & 255; break;
    case 4: { const int y = (a >> 1) + (b >> 1); x = (y * g1 + dist[y] * g2) >> 8; } break;
    case 7: { const int y = (a >> 1) + (b >> 1); x = (y * g1 + ((y + 128) & 255) * g2) >> 8; } break;
    case 8: { const int s = g2 >> 5, m = 255 - ((1 << s) - 1); x = ((a >> 1) + (b >> 1)) & m; } break;
    default: x = (a * g1 + b * g2) >> 8;
  }
  const int sb = P[k_sub];
  if (sb < 6) {
    double si = inc[0] / 2; int s = sb; if (s >= 3) { si /= 2; s -= 3; }
    subPh += si; if (subPh >= 1) subPh -= 1; int v;
    if (s != 1) v = subPh < (s == 0 ? .5 : .25) ? 0 : 255; else { const int t = (int)(subPh * 512) & 255; v = subPh >= .5 ? t : 255 - t; }
    x = (x * (255 - subG) + v * subG) >> 8;
  } else if (trc > 0) { int sg = subG; if (sg < 128) sg <<= 1; const int v = trGen(sb), amp = (trGain * sg) >> 8; x = (x * (255 - amp) + v * amp) >> 8; }
  if (P[k_op] == 5 || P[k_op] == 6) { const int dec = P[k_op] == 5 ? 4 : 8; if (crushC == 0) crushV = x; else x = crushV; crushC = (crushC + 1) % dec; }
  noise = (noise * 73 + 1) & 255;
  x = (x * (255 - noiseG) + noise * noiseG) >> 8;
  return x;
}

void Engine::process(float* L, float* R, int n) {
  if (!hasPatch) { for (int i = 0; i < n; ++i) { L[i] = 0; if (R) R[i] = 0; } return; }
  for (int i = 0; i < n; ++i) {
    cAcc += cInc; if (cAcc >= 1) { cAcc -= 1; tick(); }
    dAcc += dInc; while (dAcc >= 1) { dAcc -= 1; dOut = digital(); }
    double y = filt((dOut - 128) / 128.0);
    vcaF += (vcaB / 255.0 - vcaF) * vcaC; y *= vcaF;
    const double yy = y - dcx + .995 * dcy; dcx = y; dcy = yy;
    double out = yy * vol * .9; out = out > 1.5 ? 1 : out < -1.5 ? -1 : out - out * out * out / 6.75;
    if (!std::isfinite(out)) { out = 0; f1 = F1(); f2 = F2(); dcx = dcy = 0; }
    L[i] = (float)out; if (R) R[i] = (float)out;
  }
}

} // namespace shruthi
