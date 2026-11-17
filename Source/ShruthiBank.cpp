// Factory bank access and patch decoding. GPL v3.0.
#include "ShruthiBank.h"
#include "ShruthiBankData.h"
#include <algorithm>
#include <string>

namespace shruthi {

int numFactoryPrograms() { return shruthi_bank::kNumPrograms; }
const char* factoryName(int i) { return (i >= 0 && i < shruthi_bank::kNumPrograms) ? shruthi_bank::kPrograms[i].name : ""; }
const char* factoryCategory(int i) { return (i >= 0 && i < shruthi_bank::kNumPrograms) ? shruthi_bank::kPrograms[i].category : ""; }

static std::vector<uint8_t> hexBytes(const char* h) {
  std::vector<uint8_t> b; auto nib = [](char c){ return c <= '9' ? c - '0' : (c | 32) - 'a' + 10; };
  for (const char* p = h; p[0] && p[1]; p += 2) b.push_back((uint8_t)((nib(p[0]) << 4) | nib(p[1])));
  return b;
}
static int cl(int v, int a, int b) { return std::max(a, std::min(b, v)); }
static int s8(int v) { return v > 127 ? v - 256 : v; }

bool decodePatch(const std::vector<uint8_t>& in, const std::vector<uint8_t>* seq, Patch& P, const Patch* keep) {
  if (in.size() < 92) return false;
  std::vector<uint8_t> b(in.begin(), in.begin() + 92);
  if (b[89] == 0x6e && b[90] == 0x67) for (int i = 84; i < 92; ++i) b[i] = 0;
  P = Patch();
  int* v = P.v;
  v[k_o1s] = cl(b[0],0,34); v[k_o1p] = cl(b[1],0,127); v[k_o1r] = cl(s8(b[2]),-24,24); v[k_op] = cl(b[3],0,13);
  v[k_o2s] = cl(b[4],0,34); v[k_o2p] = cl(b[5],0,127); v[k_o2r] = cl(s8(b[6]),-24,24); v[k_o2d] = cl(b[7],0,127);
  v[k_bal] = cl(b[8],0,63); v[k_subl] = cl(b[9],0,63); v[k_nse] = cl(b[10],0,63); v[k_sub] = cl(b[11],0,10);
  v[k_cut] = cl(b[12],0,127); v[k_res] = cl(b[13],0,63); v[k_fenv] = cl(s8(b[14]),-63,63); v[k_flfo] = cl(s8(b[15]),-63,63);
  const int envK[8] = {k_e1a,k_e1d,k_e1s,k_e1r,k_e2a,k_e2d,k_e2s,k_e2r};
  for (int i = 0; i < 8; ++i) v[envK[i]] = cl(b[16 + i], 0, 127);
  const int lK[2][4] = {{k_l1w,k_l1r,k_l1a,k_l1m},{k_l2w,k_l2r,k_l2a,k_l2m}};
  for (int j = 0; j < 2; ++j) { const int o = 24 + j * 4; v[lK[j][0]] = cl(b[o],0,20); v[lK[j][1]] = cl(b[o+1],0,143); v[lK[j][2]] = cl(b[o+2],0,127); v[lK[j][3]] = cl(b[o+3],0,3); }
  for (int i = 0; i < 12; ++i) { P.mtx[i][0] = cl(b[32+i*3],0,31); P.mtx[i][1] = cl(b[33+i*3],0,26); P.mtx[i][2] = cl(s8(b[34+i*3]),-63,63); }
  std::string name; for (int i = 68; i < 76; ++i) { char c = (char)b[i]; name += (c >= 32 && c < 127) ? c : '_'; }
  while (!name.empty() && name.back() == ' ') name.pop_back();
  P.name = name;
  v[k_cut2] = cl(b[84],0,127); v[k_res2] = cl(b[85],0,63); v[k_fm1] = cl(b[86] >> 4,0,14); v[k_fm2] = cl(b[86] & 15,0,15);
  const int od[4] = {b[87], b[88], b[89], b[90]};
  const int qK[2][3] = {{k_q1a,k_q1b,k_q1o},{k_q2a,k_q2b,k_q2o}};
  for (int j = 0; j < 2; ++j) {
    const int x = od[j*2], y = od[j*2+1]; int a = x & 0x3f, bb = y >> 3, o = y & 7;
    if (x & 0x80) o |= 8;
    if (x & 0x40) bb |= 0x20;
    v[qK[j][0]] = cl(a,0,31); v[qK[j][1]] = cl(bb,0,31); v[qK[j][2]] = cl(o,0,9);
  }
  const bool hasSeq = b[91] == 0x25;
  if (hasSeq) {
    v[k_lgt] = (b[76] & 0x40) ? 1 : 0; v[k_port] = cl(b[76] & 0x3f,0,63); v[k_mode] = cl(b[77],0,2);
    v[k_bpm] = b[78] < 40 ? 120 : cl(b[78],40,240); v[k_grv] = cl(b[79],0,5); v[k_gam] = cl(b[80],0,127);
    v[k_adir] = cl(b[81] >> 4,0,4); v[k_arng] = cl(b[81] & 15,1,4); v[k_apat] = cl(b[82],0,15); v[k_div] = cl(b[83],0,11);
  } else if (keep) {
    const int ks[10] = {k_mode,k_bpm,k_grv,k_gam,k_adir,k_arng,k_apat,k_div,k_port,k_lgt};
    for (int k : ks) v[k] = keep->v[k];
  }
  if (seq && seq->size() >= 32) {
    for (int i = 0; i < 16; ++i) {
      const int a = (*seq)[i*2], c = (*seq)[i*2+1];
      P.steps[i].g = (a & 0x80) ? ((c & 0x80) ? 2 : 1) : 0; P.steps[i].n = a & 0x7f; P.steps[i].v = (c & 0x70) >> 4; P.steps[i].c = c & 15;
    }
    P.v[k_len] = keep ? keep->v[k_len] : 16;
  } else if (keep) { for (int i = 0; i < 16; ++i) P.steps[i] = keep->steps[i]; P.v[k_len] = keep->v[k_len]; }
  return hasSeq;
}

bool loadFactory(int i, Patch& out, const Patch& current) {
  if (i < 0 || i >= shruthi_bank::kNumPrograms) return false;
  const auto& e = shruthi_bank::kPrograms[i];
  auto p = hexBytes(e.patch), s = hexBytes(e.sequence);
  decodePatch(p, &s, out, &current);
  out.name = e.name;
  return true;
}

} // namespace shruthi
