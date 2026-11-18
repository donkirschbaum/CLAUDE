// Model of Anushri's analog signal path:
//   VCO (saw + pulse, linear FM from DCO, hard sync to DCO)
//   sub oscillator (DCO / -1 oct / -2 oct square)
//   mixer -> 12 dB/oct multimode VCF (LP/BP/HP) -> fuzz -> VCA
// Band-limited with PolyBLEP, processed at 2x oversampling.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

namespace anu {

static inline float ThisBlepSample(float t) { return 0.5f * t * t; }
static inline float NextBlepSample(float t) { t = 1.0f - t; return -0.5f * t * t; }

static inline float SoftClip(float x) {
  if (x < -3.0f) return -1.0f;
  if (x > 3.0f) return 1.0f;
  return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

static inline float NoteToHz(float note) {
  return 440.0f * std::exp2((note - 69.0f) / 12.0f);
}

struct AnalogKnobs {
  float fm = 0.0f;          // 0..1
  float pw = 0.5f;          // 0..1, 0.5 = square
  bool sync = false;
  float mix = 0.0f;         // 0 = saw, 0.5 = both, 1 = square
  float sub_level = 0.0f;   // 0..1
  int sub_source = 1;       // 0 = DCO, 1 = -1 oct, 2 = -2 oct
  float cutoff = 1.0f;      // 0..1 -> 18 Hz .. 22 kHz
  float resonance = 0.0f;   // 0..1
  int vcf_mode = 0;         // 0 = LP, 1 = BP, 2 = HP
  bool fuzz = false;
};

// 2x decimator: 31-tap half-band FIR.
class Decimator2x {
 public:
  void Reset() { std::memset(buf_, 0, sizeof(buf_)); pos_ = 0; }
  float Process(float a, float b) {
    Push(a);
    Push(b);
    float acc = 0.0f;
    for (int i = 0; i < kTaps; ++i) {
      acc += coeffs_[i] * buf_[(pos_ + i) % kTaps];
    }
    return acc;
  }
  void Init() {
    // Windowed-sinc half-band low-pass, cutoff at fs/4 of the oversampled rate.
    const int c = kTaps / 2;
    double sum = 0.0;
    for (int i = 0; i < kTaps; ++i) {
      int n = i - c;
      double sinc = n == 0 ? 0.5 : std::sin(M_PI * n * 0.5) / (M_PI * n);
      double w = 0.42 - 0.5 * std::cos(2.0 * M_PI * i / (kTaps - 1)) +
                 0.08 * std::cos(4.0 * M_PI * i / (kTaps - 1));
      coeffs_[i] = static_cast<float>(sinc * w);
      sum += coeffs_[i];
    }
    for (int i = 0; i < kTaps; ++i) coeffs_[i] = static_cast<float>(coeffs_[i] / sum);
    Reset();
  }

 private:
  static const int kTaps = 31;
  void Push(float x) {
    buf_[pos_] = x;
    pos_ = (pos_ + 1) % kTaps;
  }
  float coeffs_[kTaps];
  float buf_[kTaps];
  int pos_ = 0;
};

class AnalogVoice {
 public:
  void Init(double host_rate) {
    host_rate_ = host_rate;
    oversampling_ = host_rate < 88000.0 ? 2 : 1;
    rate_ = static_cast<float>(host_rate * oversampling_);
    vco_phase_ = dco_phase_ = 0.0f;
    saw_next_ = pulse_next_ = sub_next_ = dco_next_ = 0.0f;
    pulse_state_ = sub_state_ = dco_state_ = false;
    sub_counter_ = 0;
    s1_ = s2_ = 0.0f;
    fuzz_lp_ = 0.0f;
    fm_lp_ = 0.0f;
    dc_x_ = dc_y_ = 0.0f;
    drift_ = drift_target_ = 0.0f;
    drift_counter_ = 0;
    rng_ = 0x12345678u;
    std::memset(&smooth_, 0, sizeof(smooth_));
    smooth_.cutoff = 1.0f;
    smooth_.pw = 0.5f;
    cv_vco_ = cv_dco_ = 60.0f;
    cv_pw_ = cv_vcf_ = cv_vca_ = 0.0f;
    decimator_.Init();
    // Smoothing coefficients.
    knob_coef_ = 1.0f - std::exp(-1.0f / (0.008f * rate_));
    cv_coef_ = 1.0f - std::exp(-1.0f / (0.0004f * rate_));
    pitch_coef_ = 1.0f - std::exp(-1.0f / (0.00015f * rate_));
    dc_coef_ = 1.0f - static_cast<float>(2.0 * M_PI * 8.0 / host_rate);
  }

  // Targets from the digital section, updated at control rate.
  void SetControlVoltages(float vco_note, float dco_note, float pw, float vcf_offset, float vca) {
    t_vco_ = vco_note;
    t_dco_ = dco_note;
    t_pw_ = pw;
    t_vcf_ = vcf_offset;
    t_vca_ = vca;
  }

  void SetKnobs(const AnalogKnobs& k) { knobs_ = k; }

  // Renders one sample at the host rate.
  float Process() {
    float out;
    if (oversampling_ == 2) {
      float a = RenderOversampled();
      float b = RenderOversampled();
      out = decimator_.Process(a, b);
    } else {
      out = RenderOversampled();
    }
    // DC blocker (AC coupled output).
    float y = out - dc_x_ + dc_coef_ * dc_y_;
    dc_x_ = out;
    dc_y_ = y;
    return y;
  }

 private:
  float Noise() {
    rng_ = rng_ * 1664525u + 1013904223u;
    return static_cast<float>(static_cast<int32_t>(rng_)) / 2147483648.0f;
  }

  float RenderOversampled() {
    // --- smoothing of knobs and CVs
    smooth_.fm += knob_coef_ * (knobs_.fm - smooth_.fm);
    smooth_.pw += knob_coef_ * (knobs_.pw - smooth_.pw);
    smooth_.mix += knob_coef_ * (knobs_.mix - smooth_.mix);
    smooth_.sub += knob_coef_ * (knobs_.sub_level - smooth_.sub);
    smooth_.cutoff += knob_coef_ * (knobs_.cutoff - smooth_.cutoff);
    smooth_.res += knob_coef_ * (knobs_.resonance - smooth_.res);
    cv_vco_ += pitch_coef_ * (t_vco_ - cv_vco_);
    cv_dco_ += pitch_coef_ * (t_dco_ - cv_dco_);
    cv_pw_ += cv_coef_ * (t_pw_ - cv_pw_);
    cv_vcf_ += cv_coef_ * (t_vcf_ - cv_vcf_);
    cv_vca_ += cv_coef_ * (t_vca_ - cv_vca_);

    // --- slow analog drift of the VCO (a few cents)
    if (++drift_counter_ >= 2048) {
      drift_counter_ = 0;
      drift_target_ = Noise() * 0.035f;
    }
    drift_ += 0.0005f * (drift_target_ - drift_);

    // --- DCO (digital square, used as sub source, FM and sync master)
    float dco_freq = NoteToHz(cv_dco_) / rate_;
    if (dco_freq > 0.45f) dco_freq = 0.45f;
    float dco_this = dco_next_;
    dco_next_ = 0.0f;
    bool sync_event = false;
    float sync_t = 0.0f;  // time elapsed since the DCO reset, in samples
    dco_phase_ += dco_freq;
    if (!dco_state_ && dco_phase_ >= 0.5f) {
      float t = (dco_phase_ - 0.5f) / dco_freq;
      dco_state_ = true;
      dco_this += -1.0f * ThisBlepSample(t);
      dco_next_ += -1.0f * NextBlepSample(t);
    }
    if (dco_phase_ >= 1.0f) {
      dco_phase_ -= 1.0f;
      float t = dco_phase_ / dco_freq;
      dco_state_ = false;
      dco_this += 1.0f * ThisBlepSample(t);
      dco_next_ += 1.0f * NextBlepSample(t);
      sync_event = knobs_.sync;
      sync_t = t;
    }
    dco_next_ += dco_state_ ? 0.0f : 1.0f;
    float dco = 2.0f * dco_this - 1.0f;  // bipolar square

    // --- VCO frequency with linear FM from the DCO
    fm_lp_ += 0.25f * (dco - fm_lp_);
    float vco_hz = NoteToHz(cv_vco_ + drift_);
    float fm_depth = smooth_.fm * smooth_.fm * 2.5f;
    vco_hz *= 1.0f + fm_depth * fm_lp_;
    if (vco_hz < 0.0f) vco_hz = 0.0f;
    float freq = vco_hz / rate_;
    if (freq > 0.45f) freq = 0.45f;
    float pw = 0.5f + (smooth_.pw - 0.5f) * 0.92f + cv_pw_ * 0.46f;
    if (pw < 0.02f) pw = 0.02f;
    if (pw > 0.98f) pw = 0.98f;

    float saw_this = saw_next_;
    float pulse_this = pulse_next_;
    float sub_this = sub_next_;
    saw_next_ = pulse_next_ = sub_next_ = 0.0f;

    // Advance VCO, handling natural wraps, pulse edges and sync resets in
    // chronological order inside this sample.
    float phase = vco_phase_;
    float remaining = 1.0f;     // fraction of the sample left to process
    float sync_at = sync_event ? 1.0f - sync_t : 2.0f;  // when the reset happens
    float elapsed = 0.0f;
    int guard = 0;
    while (remaining > 0.0f && guard++ < 8) {
      // Next event candidates (in fraction of a sample from now).
      float to_pw = !pulse_state_ ? (phase >= pw ? 0.0f : (freq > 0.0f ? (pw - phase) / freq : 2.0f)) : 2.0f;
      float to_wrap = freq > 0.0f ? (1.0f - phase) / freq : 2.0f;
      float to_sync = sync_at - elapsed;
      float next = to_pw;
      int kind = 0;
      if (to_wrap < next) { next = to_wrap; kind = 1; }
      if (to_sync < next) { next = to_sync; kind = 2; }
      if (next > remaining) {
        phase += freq * remaining;
        break;
      }
      phase += freq * next;
      elapsed += next;
      remaining -= next;
      float t = remaining;  // time since the event at the end of this sample
      if (kind == 0) {
        // pulse falls 1 -> 0
        pulse_state_ = true;
        pulse_this += -1.0f * ThisBlepSample(t);
        pulse_next_ += -1.0f * NextBlepSample(t);
      } else {
        // wrap or sync reset: saw jumps to 0, pulse rises to 1
        float saw_jump = -phase;
        if (kind == 1) saw_jump = -1.0f;
        saw_this += saw_jump * ThisBlepSample(t);
        saw_next_ += saw_jump * NextBlepSample(t);
        if (pulse_state_) {
          pulse_this += 1.0f * ThisBlepSample(t);
          pulse_next_ += 1.0f * NextBlepSample(t);
          pulse_state_ = false;
        }
        // sub-oscillator flip-flop clocked by the VCO reset
        ++sub_counter_;
        int div = knobs_.sub_source == 2 ? 2 : 1;
        if (sub_counter_ >= div) {
          sub_counter_ = 0;
          float d = sub_state_ ? -1.0f : 1.0f;
          sub_state_ = !sub_state_;
          sub_this += d * ThisBlepSample(t);
          sub_next_ += d * NextBlepSample(t);
        }
        phase = 0.0f;
        if (kind == 2) sync_at = 2.0f;
      }
    }
    vco_phase_ = phase;
    saw_next_ += phase;
    pulse_next_ += pulse_state_ ? 0.0f : 1.0f;
    sub_next_ += sub_state_ ? 1.0f : 0.0f;

    float saw = 2.0f * saw_this - 1.0f;
    float pulse = 2.0f * pulse_this - 1.0f;
    float sub = knobs_.sub_source == 0 ? dco : 2.0f * sub_this - 1.0f;

    // --- mixer
    float m = smooth_.mix;
    float saw_gain = m < 0.5f ? 1.0f : 2.0f * (1.0f - m);
    float pulse_gain = m > 0.5f ? 1.0f : 2.0f * m;
    float mixed = 0.6f * (saw_gain * saw + pulse_gain * pulse) + 0.7f * smooth_.sub * sub;
    mixed = SoftClip(mixed * 0.6f) * 1.67f;

    // --- VCF: 12 dB/oct state-variable filter with saturating integrator
    float cutoff_hz = 18.0f * std::pow(22000.0f / 18.0f, smooth_.cutoff);
    cutoff_hz *= std::exp2(cv_vcf_ / 12.0f);
    float max_hz = 0.42f * rate_;
    if (max_hz > 24000.0f) max_hz = 24000.0f;
    if (cutoff_hz > max_hz) cutoff_hz = max_hz;
    if (cutoff_hz < 8.0f) cutoff_hz = 8.0f;
    float g = std::tan(static_cast<float>(M_PI) * cutoff_hz / rate_);
    float r = smooth_.res;
    float k = 2.0f * (1.0f - 1.04f * r * r * (3.0f - 2.0f * r));  // damping
    float x = mixed;
    float hp = (x - (k + g) * s1_ - s2_) / (1.0f + k * g + g * g);
    float bp = g * hp + s1_;
    s1_ = SoftClip(g * hp + bp);
    float lp = g * bp + s2_;
    s2_ = g * bp + lp;
    float filtered = knobs_.vcf_mode == 0 ? lp : (knobs_.vcf_mode == 1 ? bp : hp);
    // Resonance makes the OTA filter loose some low-end, keep levels sane.
    filtered *= 1.0f - 0.25f * r;

    // --- fuzz (between VCF and VCA)
    if (knobs_.fuzz) {
      float f = std::tanh(filtered * 7.0f + 0.3f) - std::tanh(0.3f);
      f = std::tanh(f * 2.5f);
      fuzz_lp_ += (1.0f - std::exp(-2.0f * static_cast<float>(M_PI) * 5500.0f / rate_)) * (f - fuzz_lp_);
      filtered = fuzz_lp_ * 0.8f;
    }

    // --- VCA
    float vca = cv_vca_;
    return filtered * vca * 0.6f;
  }

  struct Smooth {
    float fm, pw, mix, sub, cutoff, res;
  } smooth_;
  AnalogKnobs knobs_;
  double host_rate_ = 48000.0;
  int oversampling_ = 2;
  float rate_ = 96000.0f;
  float t_vco_ = 60.0f, t_dco_ = 60.0f, t_pw_ = 0.0f, t_vcf_ = 0.0f, t_vca_ = 0.0f;
  float cv_vco_, cv_dco_, cv_pw_, cv_vcf_, cv_vca_;
  float vco_phase_, dco_phase_;
  float saw_next_, pulse_next_, sub_next_, dco_next_;
  bool pulse_state_, sub_state_, dco_state_;
  int sub_counter_;
  float s1_, s2_;
  float fuzz_lp_, fm_lp_;
  float dc_x_, dc_y_, dc_coef_;
  float drift_, drift_target_;
  int drift_counter_;
  uint32_t rng_;
  float knob_coef_, cv_coef_, pitch_coef_;
  Decimator2x decimator_;
};

}  // namespace anu
