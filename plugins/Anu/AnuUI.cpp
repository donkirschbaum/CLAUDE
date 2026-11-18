// Anu - panel UI (NanoVG), laid out after the Anushri front panel:
// analog section on top, 10 paged "soft" pots below (SYNTH / KBD-CLK / DRUMS),
// sequencer buttons, drum pattern grid and a MIDI setup page.
// GPLv3.

#include "DistrhoUI.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "params.h"
#include "resources.h"

START_NAMESPACE_DISTRHO

using namespace anu;

static const float kBaseW = 1100.0f;
static const float kBaseH = 640.0f;

enum ControlType { CT_KNOB, CT_SMALL_KNOB, CT_SWITCH, CT_BUTTON };

struct Control {
  int param;
  ControlType type;
  float x, y;
  int page;  // -1 = always visible, otherwise page index
};

enum Page { PAGE_SYNTH, PAGE_KBD, PAGE_DRUMS, PAGE_SETUP, PAGE_COUNT };
static const char* kPageNames[PAGE_COUNT] = { "SYNTH", "KBD / CLK", "DRUMS", "SETUP" };

static const int kPageParams[3][10] = {
  { P_GLIDE, P_VCO_RANGE, P_VCO_DETUNE, P_LFO_SHAPE, P_LFO_RATE,
    P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE, P_VCA_ENV },
  { P_TEMPO, P_SWING, P_ARP_MODE, P_ARP_PATTERN, P_ACIDITY,
    P_DCO_RANGE, P_DCO_FINE, P_VIBRATO_RATE, P_VIBRATO_DEST, P_VELOCITY_MOD },
  { P_DRUMS_X, P_BD_DENSITY, P_SD_DENSITY, P_HH_DENSITY, P_DRUMS_BALANCE,
    P_DRUMS_Y, P_BD_TONE, P_SD_TONE, P_HH_TONE, P_DRUMS_BANDWIDTH },
};

static const int kSetupParams[14] = {
  P_MIDI_IN_CHANNEL, P_MIDI_OUT_CHANNEL, P_MIDI_OUT_SEQ, P_MIDI_THRU, P_CUTOFF_BIAS, P_CUTOFF_TRACKING, P_LEGATO,
  P_MIDI_OUT_DRUMS, P_DRUM_OUT_CHANNEL, P_BD_NOTE, P_SD_NOTE, P_HH_NOTE, P_DRUM_GATE_MS, -1,
};

static const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

static std::string FormatTime(double seconds) {
  char buf[32];
  if (seconds < 1.0) std::snprintf(buf, sizeof(buf), "%.0f ms", seconds * 1000.0);
  else std::snprintf(buf, sizeof(buf), "%.2f s", seconds);
  return buf;
}

static std::string FormatValue(int index, float v) {
  const ParamInfo& p = kParams[index];
  char buf[64];
  const double control_rate = 20000000.0 / 510.0 / 16.0;
  const int v8 = static_cast<int>(std::lround(v * 255.0f));
  switch (index) {
    case P_CUTOFF: {
      double hz = 18.0 * std::pow(22000.0 / 18.0, v);
      if (hz < 1000.0) std::snprintf(buf, sizeof(buf), "%.0f Hz", hz);
      else std::snprintf(buf, sizeof(buf), "%.1f kHz", hz / 1000.0);
      return buf;
    }
    case P_PW:
      std::snprintf(buf, sizeof(buf), "%.0f %%", 50.0 + (v - 0.5) * 92.0);
      return buf;
    case P_MIX:
      if (v < 0.02f) return "Saw";
      if (v > 0.98f) return "Square";
      std::snprintf(buf, sizeof(buf), "Saw %d / Sq %d", static_cast<int>(std::lround(std::fmin(1.0, 2.0 * (1.0 - v)) * 100)),
                    static_cast<int>(std::lround(std::fmin(1.0, 2.0 * v) * 100)));
      return buf;
    case P_VCO_MOD:
    case P_PW_MOD:
    case P_VELOCITY_MOD: {
      const char* neg = index == P_VELOCITY_MOD ? "VCF" : "Env";
      const char* pos = index == P_VELOCITY_MOD ? "VCA" : "LFO";
      if (std::fabs(v) < 0.03f) return "Off";
      std::snprintf(buf, sizeof(buf), "%s %d %%", v < 0 ? neg : pos, static_cast<int>(std::lround(std::fabs(v) * 100)));
      return buf;
    }
    case P_CUTOFF_BIAS:
      std::snprintf(buf, sizeof(buf), "%+.0f st", v * 64.0);
      return buf;
    case P_LFO_RATE: {
      if (v8 < 2) return "Clock sync";
      double hz = lut_res_lfo_increments[v8] * control_rate / 4294967296.0;
      std::snprintf(buf, sizeof(buf), hz < 10 ? "%.2f Hz" : "%.1f Hz", hz);
      return buf;
    }
    case P_VIBRATO_RATE: {
      double hz = lut_res_lfo_increments[96 + (v8 >> 1)] * control_rate / 4294967296.0;
      std::snprintf(buf, sizeof(buf), "%.2f Hz", hz);
      return buf;
    }
    case P_VIBRATO_DEST:
      if (v < 0.5f) std::snprintf(buf, sizeof(buf), "Vib + %d%% growl", static_cast<int>(v * 200));
      else std::snprintf(buf, sizeof(buf), "Growl + %d%% vib", static_cast<int>((1.0f - v) * 200));
      return buf;
    case P_ATTACK:
    case P_DECAY:
    case P_RELEASE:
      return FormatTime(4294967296.0 / (static_cast<double>(lut_res_env_increments[v8]) * control_rate));
    case P_GLIDE:
      if (v8 == 0) return "Off";
      return FormatTime(65536.0 / (static_cast<double>(lut_res_glide_increments[v8]) * control_rate));
    case P_VCA_ENV:
      if (v8 < 4) return "= VCF env";
      if (v8 > 250) return "Gate";
      std::snprintf(buf, sizeof(buf), "Morph %d %%", static_cast<int>(v * 100));
      return buf;
    case P_TEMPO:
      if (v < 39.5f) return "EXT (Host)";
      std::snprintf(buf, sizeof(buf), "%d BPM", static_cast<int>(std::lround(v)));
      return buf;
    case P_VCO_RANGE:
      std::snprintf(buf, sizeof(buf), "%+d st", static_cast<int>(std::lround(v)));
      return buf;
    case P_DCO_RANGE:
      std::snprintf(buf, sizeof(buf), "%+d oct", static_cast<int>(std::lround(v)));
      return buf;
    case P_DRUMS_BALANCE:
      std::snprintf(buf, sizeof(buf), "BD %d / SD %d", v8 < 128 ? 100 : (255 - (v8 - 128) * 2) * 100 / 255,
                    v8 >= 128 ? 100 : (v8 * 2) * 100 / 255);
      return buf;
    case P_MIDI_IN_CHANNEL:
      if (v < 0.5f) return "Omni";
      std::snprintf(buf, sizeof(buf), "Ch %d", static_cast<int>(std::lround(v)));
      return buf;
    case P_MIDI_OUT_CHANNEL:
    case P_DRUM_OUT_CHANNEL:
      std::snprintf(buf, sizeof(buf), "Ch %d", static_cast<int>(std::lround(v)));
      return buf;
    case P_BD_NOTE:
    case P_SD_NOTE:
    case P_HH_NOTE: {
      int n = static_cast<int>(std::lround(v));
      std::snprintf(buf, sizeof(buf), "%s%d (%d)", kNoteNames[n % 12], n / 12 - 2, n);
      return buf;
    }
    case P_DRUM_GATE_MS:
      std::snprintf(buf, sizeof(buf), "%d ms", static_cast<int>(std::lround(v)));
      return buf;
    default:
      break;
  }
  switch (p.kind) {
    case KIND_KNOB:
      std::snprintf(buf, sizeof(buf), "%d %%", static_cast<int>(std::lround(v * 100)));
      return buf;
    case KIND_BIPOLAR:
      std::snprintf(buf, sizeof(buf), "%+d %%", static_cast<int>(std::lround(v * 100)));
      return buf;
    case KIND_ENUM:
    case KIND_BOOL: {
      int i = static_cast<int>(std::lround(v - p.min));
      if (i < 0) i = 0;
      if (i > static_cast<int>(p.max - p.min)) i = static_cast<int>(p.max - p.min);
      return p.enums[i];
    }
    default:
      std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::lround(v)));
      return buf;
  }
}

class AnuUI : public UI {
 public:
  AnuUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT, true) {
    loadSharedResources();
    for (int i = 0; i < static_cast<int>(P_COUNT); ++i) values_[i] = kParams[i].def;
    BuildControls();
    setGeometryConstraints(DISTRHO_UI_DEFAULT_WIDTH / 2, DISTRHO_UI_DEFAULT_HEIGHT / 2, true);
  }

 protected:
  // ------------------------------------------------------------------ DSP
  void parameterChanged(uint32_t index, float value) override {
    if (index >= P_COUNT) return;
    if (values_[index] != value) {
      values_[index] = value;
      if (learning_ == static_cast<int>(index)) {
        learning_ = -1;
        status_ = std::string("MIDI CC assigned to ") + kParams[index].name;
      }
      repaint();
    }
  }

  void stateChanged(const char*, const char*) override {}

  // ------------------------------------------------------------------ drawing
  void onNanoDisplay() override {
    const float s = static_cast<float>(getWidth()) / kBaseW;
    scale_ = s;
    save();
    scale(s, s);
    fontFace(NANOVG_DEJAVU_SANS_TTF);

    DrawBackground();
    for (const Control& c : controls_) {
      if (c.page >= 0 && c.page != page_) continue;
      DrawControl(c);
    }
    DrawPageButtons();
    DrawTransport();
    if (page_ == PAGE_DRUMS) DrawDrumGrid();
    else DrawSequenceInfo();
    DrawStatus();
    restore();
  }

  // ------------------------------------------------------------------ input
  bool onMouse(const MouseEvent& ev) override {
    const float x = static_cast<float>(ev.pos.getX()) / scale_;
    const float y = static_cast<float>(ev.pos.getY()) / scale_;

    if (!ev.press) {
      if (drag_param_ >= 0) {
        editParameter(drag_param_, false);
        drag_param_ = -1;
        repaint();
        return true;
      }
      return false;
    }

    // Page buttons.
    for (int i = 0; i < PAGE_COUNT; ++i) {
      if (Inside(x, y, 22, 330 + i * 52, 130, 42)) {
        page_ = i;
        repaint();
        return true;
      }
    }
    // Transport buttons.
    if (ev.button == kMouseButtonLeft) {
      if (Inside(x, y, 840, 330, 110, 42)) { ToggleParam(P_RECORD); return true; }
      if (Inside(x, y, 960, 330, 110, 42)) { ToggleParam(P_RUN); return true; }
      if (Inside(x, y, 840, 382, 110, 42)) {
        if (values_[P_RECORD] > 0.5f) setState("cmd", "tie");
        else ToggleParam(P_HOLD);
        return true;
      }
      if (Inside(x, y, 960, 382, 110, 42)) {
        setState("cmd", "rest");
        status_ = values_[P_RECORD] > 0.5f ? "Rest inserted" : "REST works while recording";
        repaint();
        return true;
      }
      if (Inside(x, y, 1010, 18, 70, 24)) {
        setState("cmd", "panic");
        status_ = "All notes off";
        repaint();
        return true;
      }
      if (page_ == PAGE_DRUMS) {
        if (Inside(x, y, 840, 448, 110, 26)) { ToggleParam(P_DRUM_OVERRIDE); return true; }
        if (Inside(x, y, 960, 448, 110, 26)) {
          for (int i = 0; i < 3; ++i) {
            char cmd[32];
            std::snprintf(cmd, sizeof(cmd), "clear %d", i);
            setState("cmd", cmd);
          }
          return true;
        }
        for (int inst = 0; inst < 3; ++inst) {
          for (int step = 0; step < 16; ++step) {
            float cx = 868 + step * 13.2f;
            float cy = 495 + inst * 30;
            if (Inside(x, y, cx, cy, 12, 22)) {
              char cmd[32];
              std::snprintf(cmd, sizeof(cmd), "toggle %d %d", inst, step);
              setState("cmd", cmd);
              status_ = "Pattern edited - enable OVERRIDE to play the programmed pattern";
              repaint();
              return true;
            }
          }
        }
      }
    }

    const int hit = HitTest(x, y);
    if (hit < 0) return false;
    const Control& c = controls_[hit];

    if (ev.button == kMouseButtonRight) {
      if (ev.mod & kModifierShift) {
        char cmd[32];
        std::snprintf(cmd, sizeof(cmd), "forget %d", c.param);
        setState("cmd", cmd);
        status_ = std::string("MIDI learn cleared for ") + kParams[c.param].name;
        learning_ = -1;
      } else if (learning_ == c.param) {
        setState("cmd", "learn -1");
        learning_ = -1;
        status_ = "MIDI learn cancelled";
      } else {
        char cmd[32];
        std::snprintf(cmd, sizeof(cmd), "learn %d", c.param);
        setState("cmd", cmd);
        learning_ = c.param;
        status_ = std::string("MIDI LEARN: move a controller for ") + kParams[c.param].name +
                  "  (right-click again to cancel)";
      }
      repaint();
      return true;
    }
    if (ev.button != kMouseButtonLeft) return false;

    const ParamInfo& p = kParams[c.param];
    if (c.type == CT_SWITCH || p.kind == KIND_BOOL) {
      float v = values_[c.param] + 1.0f;
      if (v > p.max) v = p.min;
      SetParam(c.param, v, true);
      return true;
    }
    // Double click resets.
    if (ev.time - last_click_time_ < 350 && last_click_param_ == c.param) {
      SetParam(c.param, p.def, true);
      last_click_param_ = -1;
      return true;
    }
    last_click_time_ = ev.time;
    last_click_param_ = c.param;
    drag_param_ = c.param;
    drag_y_ = y;
    drag_norm_ = Normalize(c.param, values_[c.param]);
    editParameter(c.param, true);
    repaint();
    return true;
  }

  bool onMotion(const MotionEvent& ev) override {
    const float x = static_cast<float>(ev.pos.getX()) / scale_;
    const float y = static_cast<float>(ev.pos.getY()) / scale_;
    if (drag_param_ >= 0) {
      const float sensitivity = (ev.mod & kModifierShift) ? 0.0012f : 0.0055f;
      drag_norm_ += (drag_y_ - y) * sensitivity;
      if (drag_norm_ < 0.0f) drag_norm_ = 0.0f;
      if (drag_norm_ > 1.0f) drag_norm_ = 1.0f;
      drag_y_ = y;
      float v = Denormalize(drag_param_, drag_norm_);
      if (v != values_[drag_param_]) {
        values_[drag_param_] = v;
        setParameterValue(drag_param_, v);
      }
      repaint();
      return true;
    }
    int hit = HitTest(x, y);
    int param = hit >= 0 ? controls_[hit].param : -1;
    if (param != hover_param_) {
      hover_param_ = param;
      repaint();
    }
    return false;
  }

  bool onScroll(const ScrollEvent& ev) override {
    const float x = static_cast<float>(ev.pos.getX()) / scale_;
    const float y = static_cast<float>(ev.pos.getY()) / scale_;
    int hit = HitTest(x, y);
    if (hit < 0) return false;
    const int param = controls_[hit].param;
    const ParamInfo& p = kParams[param];
    float dir = ev.delta.getY() > 0 ? 1.0f : -1.0f;
    float v = values_[param];
    if (p.kind == KIND_INT || p.kind == KIND_ENUM || p.kind == KIND_BOOL) v += dir;
    else v += dir * (p.max - p.min) * ((ev.mod & kModifierShift) ? 0.002f : 0.02f);
    if (v < p.min) v = p.min;
    if (v > p.max) v = p.max;
    SetParam(param, v, true);
    return true;
  }

 private:
  // ------------------------------------------------------------------ helpers
  static bool Inside(float x, float y, float rx, float ry, float rw, float rh) {
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
  }

  float Normalize(int param, float v) const {
    const ParamInfo& p = kParams[param];
    return (v - p.min) / (p.max - p.min);
  }

  float Denormalize(int param, float n) const {
    const ParamInfo& p = kParams[param];
    float v = p.min + n * (p.max - p.min);
    if (p.kind == KIND_INT || p.kind == KIND_ENUM || p.kind == KIND_BOOL) v = std::round(v);
    if (p.kind == KIND_BIPOLAR && std::fabs(v) < 0.025f) v = 0.0f;  // centre detent
    return v;
  }

  void SetParam(int param, float v, bool gesture) {
    values_[param] = v;
    if (gesture) editParameter(param, true);
    setParameterValue(param, v);
    if (gesture) editParameter(param, false);
    repaint();
  }

  void ToggleParam(int param) { SetParam(param, values_[param] > 0.5f ? 0.0f : 1.0f, true); }

  void BuildControls() {
    // Analog section.
    controls_.push_back({ P_FM, CT_KNOB, 70, 145, -1 });
    controls_.push_back({ P_VCO_MOD, CT_KNOB, 155, 145, -1 });
    controls_.push_back({ P_PW, CT_KNOB, 240, 145, -1 });
    controls_.push_back({ P_PW_MOD, CT_KNOB, 325, 145, -1 });
    controls_.push_back({ P_SYNC, CT_SWITCH, 70, 250, -1 });
    controls_.push_back({ P_MIX, CT_KNOB, 430, 145, -1 });
    controls_.push_back({ P_SUB_LEVEL, CT_KNOB, 515, 145, -1 });
    controls_.push_back({ P_SUB_SOURCE, CT_SWITCH, 472, 250, -1 });
    controls_.push_back({ P_CUTOFF, CT_KNOB, 630, 145, -1 });
    controls_.push_back({ P_RESONANCE, CT_KNOB, 715, 145, -1 });
    controls_.push_back({ P_VCF_ENV, CT_KNOB, 800, 145, -1 });
    controls_.push_back({ P_VCF_LFO, CT_KNOB, 885, 145, -1 });
    controls_.push_back({ P_VCF_MODE, CT_SWITCH, 757, 250, -1 });
    controls_.push_back({ P_VOLUME, CT_KNOB, 975, 145, -1 });
    controls_.push_back({ P_DRUM_LEVEL, CT_KNOB, 1050, 145, -1 });
    controls_.push_back({ P_FUZZ, CT_SWITCH, 1012, 250, -1 });
    // Soft pots.
    for (int page = 0; page < 3; ++page) {
      for (int i = 0; i < 10; ++i) {
        controls_.push_back({ kPageParams[page][i], CT_KNOB, 235.0f + (i % 5) * 132.0f,
                              400.0f + (i / 5) * 118.0f, page });
      }
    }
    // Setup page.
    for (int i = 0; i < 14; ++i) {
      if (kSetupParams[i] < 0) continue;
      const ParamInfo& p = kParams[kSetupParams[i]];
      controls_.push_back({ kSetupParams[i], p.kind == KIND_BOOL ? CT_SWITCH : CT_SMALL_KNOB,
                            215.0f + (i % 7) * 96.0f, 395.0f + (i / 7) * 120.0f, PAGE_SETUP });
    }
  }

  int HitTest(float x, float y) const {
    for (size_t i = 0; i < controls_.size(); ++i) {
      const Control& c = controls_[i];
      if (c.page >= 0 && c.page != page_) continue;
      float r = c.type == CT_KNOB ? 30.0f : (c.type == CT_SMALL_KNOB ? 22.0f : 24.0f);
      if (std::fabs(x - c.x) <= r + 6 && std::fabs(y - c.y) <= r + 6) return static_cast<int>(i);
    }
    return -1;
  }

  // ------------------------------------------------------------------ painting
  void Label(float x, float y, const char* s, float size, const Color& col, int align) {
    fontSize(size);
    fillColor(col);
    textAlign(align);
    text(x, y, s, nullptr);
  }

  void DrawBackground() {
    // Panel.
    beginPath();
    rect(0, 0, kBaseW, kBaseH);
    fillPaint(linearGradient(0, 0, 0, kBaseH, Color(46, 47, 51), Color(30, 31, 34)));
    fill();

    // Header.
    Label(24, 32, "ANU", 26, Color(240, 240, 235), ALIGN_LEFT | ALIGN_MIDDLE);
    Label(90, 33, "analog monosynth  +  generative drum machine   ·   after Mutable Instruments Anushri", 12,
          Color(160, 160, 155), ALIGN_LEFT | ALIGN_MIDDLE);

    // LEDs.
    const uint32_t leds = static_cast<uint32_t>(values_[P_OUT_LEDS]);
    DrawLed(760, 30, leds & LED_GATE, "GATE");
    DrawLed(830, 30, leds & LED_LFO, "LFO");
    DrawLed(900, 30, leds & LED_CLOCK, "BEAT");
    // Panic.
    beginPath();
    roundedRect(1010, 18, 70, 24, 4);
    fillColor(Color(60, 60, 66));
    fill();
    Label(1045, 30, "PANIC", 11, Color(220, 220, 215), ALIGN_CENTER | ALIGN_MIDDLE);

    // Section frames.
    Section(20, 62, 345, 238, "OSCILLATORS");
    Section(375, 62, 185, 238, "MIXER");
    Section(570, 62, 360, 238, "FILTER");
    Section(940, 62, 140, 238, "OUTPUT");
    Section(20, 312, 800, 300, nullptr);
    Section(830, 312, 250, 300, nullptr);

    // Signal flow arrows.
    for (float ax : { 367.5f, 562.5f, 932.5f }) {
      beginPath();
      moveTo(ax - 4, 175);
      lineTo(ax + 4, 181);
      lineTo(ax - 4, 187);
      fillColor(Color(120, 120, 115));
      fill();
    }
  }

  void Section(float x, float y, float w, float h, const char* title) {
    beginPath();
    roundedRect(x, y, w, h, 8);
    fillColor(Color(38, 39, 43));
    fill();
    strokeColor(Color(70, 71, 76));
    strokeWidth(1.0f);
    stroke();
    if (title) {
      Label(x + 12, y + 18, title, 12, Color(236, 116, 52), ALIGN_LEFT | ALIGN_MIDDLE);
    }
  }

  void DrawLed(float x, float y, bool on, const char* label) {
    beginPath();
    circle(x, y, 5);
    fillColor(on ? Color(255, 70, 50) : Color(70, 30, 28));
    fill();
    if (on) {
      beginPath();
      circle(x, y, 11);
      fillPaint(radialGradient(x, y, 3, 11, Color(255, 70, 50, 0.45f), Color(255, 70, 50, 0.0f)));
      fill();
    }
    if (label) Label(x + 10, y, label, 10, Color(170, 170, 165), ALIGN_LEFT | ALIGN_MIDDLE);
  }

  void DrawControl(const Control& c) {
    const ParamInfo& p = kParams[c.param];
    const float v = values_[c.param];
    const bool hot = c.param == hover_param_ || c.param == drag_param_;
    const bool learning = c.param == learning_;
    const Color text_col(225, 225, 220);
    const Color dim(150, 150, 145);

    if (c.type == CT_SWITCH) {
      const int positions = static_cast<int>(p.max - p.min) + 1;
      const float h = positions == 3 ? 46.0f : 34.0f;
      beginPath();
      roundedRect(c.x - 9, c.y - h / 2, 18, h, 9);
      fillColor(Color(18, 18, 20));
      fill();
      strokeColor(learning ? Color(80, 200, 255) : (hot ? Color(150, 150, 150) : Color(80, 80, 85)));
      stroke();
      const int pos = static_cast<int>(std::lround(v - p.min));
      const float ky = positions == 3 ? c.y + 15 - pos * 15 : (pos ? c.y - 8 : c.y + 8);
      beginPath();
      circle(c.x, ky, 7);
      fillPaint(linearGradient(c.x, ky - 7, c.x, ky + 7, Color(235, 235, 230), Color(150, 150, 145)));
      fill();
      // Position labels.
      for (int i = 0; i < positions; ++i) {
        const float ly = positions == 3 ? c.y + 15 - i * 15 : (i ? c.y - 8 : c.y + 8);
        Label(c.x + 15, ly, p.enums[i], 10, i == pos ? text_col : dim, ALIGN_LEFT | ALIGN_MIDDLE);
      }
      Label(c.x, c.y + h / 2 + 14, p.label, 11, learning ? Color(80, 200, 255) : text_col,
            ALIGN_CENTER | ALIGN_MIDDLE);
      return;
    }

    const float r = c.type == CT_KNOB ? 24.0f : 17.0f;
    const float a0 = 0.75f * static_cast<float>(M_PI);
    const float a1 = 2.25f * static_cast<float>(M_PI);
    const float n = Normalize(c.param, v);
    const float an = a0 + (a1 - a0) * n;
    const bool bipolar = p.kind == KIND_BIPOLAR || (p.min < 0 && p.max > 0);

    // Track.
    beginPath();
    arc(c.x, c.y, r + 6, a0, a1, CW);
    strokeColor(Color(25, 25, 28));
    strokeWidth(4.0f);
    stroke();
    // Value arc.
    const float ref = bipolar ? a0 + (a1 - a0) * Normalize(c.param, 0.0f) : a0;
    if (std::fabs(an - ref) > 0.001f) {
      beginPath();
      arc(c.x, c.y, r + 6, std::fmin(ref, an), std::fmax(ref, an), CW);
      strokeColor(learning ? Color(80, 200, 255) : Color(236, 116, 52));
      strokeWidth(3.0f);
      stroke();
    }
    // Knob body.
    beginPath();
    circle(c.x, c.y + 2, r);
    fillColor(Color(0, 0, 0, 0.5f));
    fill();
    beginPath();
    circle(c.x, c.y, r);
    fillPaint(linearGradient(c.x, c.y - r, c.x, c.y + r, hot ? Color(78, 78, 84) : Color(64, 64, 70),
                             Color(22, 22, 25)));
    fill();
    beginPath();
    circle(c.x, c.y, r * 0.78f);
    fillPaint(linearGradient(c.x, c.y - r, c.x, c.y + r, Color(40, 40, 44), Color(58, 58, 64)));
    fill();
    // Pointer.
    beginPath();
    moveTo(c.x + std::cos(an) * r * 0.25f, c.y + std::sin(an) * r * 0.25f);
    lineTo(c.x + std::cos(an) * r * 0.85f, c.y + std::sin(an) * r * 0.85f);
    strokeColor(Color(245, 245, 240));
    strokeWidth(2.5f);
    stroke();

    Label(c.x, c.y + r + 18, p.label, c.type == CT_KNOB ? 11.5f : 10.5f,
          learning ? Color(80, 200, 255) : text_col, ALIGN_CENTER | ALIGN_MIDDLE);
    // Value readout: always for paged controls, on hover for the analog section.
    if (c.page >= 0 || hot) {
      Label(c.x, c.y + r + 33, FormatValue(c.param, v).c_str(), 10, hot ? Color(255, 180, 120) : dim,
            ALIGN_CENTER | ALIGN_MIDDLE);
    }
  }

  void DrawPageButtons() {
    for (int i = 0; i < PAGE_COUNT; ++i) {
      const float y = 330 + i * 52;
      beginPath();
      roundedRect(22, y, 130, 42, 6);
      fillColor(page_ == i ? Color(236, 116, 52) : Color(58, 58, 64));
      fill();
      Label(87, y + 21, kPageNames[i], 13, page_ == i ? Color(25, 25, 25) : Color(220, 220, 215),
            ALIGN_CENTER | ALIGN_MIDDLE);
    }
    Label(87, 560, "Right-click any control:", 9.5f, Color(150, 150, 145), ALIGN_CENTER | ALIGN_MIDDLE);
    Label(87, 574, "MIDI Learn", 9.5f, Color(150, 150, 145), ALIGN_CENTER | ALIGN_MIDDLE);
    Label(87, 588, "Shift+Right: forget", 9.5f, Color(150, 150, 145), ALIGN_CENTER | ALIGN_MIDDLE);
  }

  void Button(float x, float y, float w, float h, const char* label, bool lit, const Color& led) {
    beginPath();
    roundedRect(x, y, w, h, 6);
    fillPaint(linearGradient(x, y, x, y + h, Color(72, 72, 78), Color(48, 48, 54)));
    fill();
    strokeColor(Color(90, 90, 96));
    stroke();
    beginPath();
    circle(x + 14, y + h / 2, 4.5f);
    fillColor(lit ? led : Color(55, 35, 32));
    fill();
    Label(x + w / 2 + 8, y + h / 2, label, 12, Color(230, 230, 225), ALIGN_CENTER | ALIGN_MIDDLE);
  }

  void DrawTransport() {
    const uint32_t leds = static_cast<uint32_t>(values_[P_OUT_LEDS]);
    const bool recording = values_[P_RECORD] > 0.5f;
    Button(840, 330, 110, 42, "REC", recording, Color(255, 60, 50));
    Button(960, 330, 110, 42, "RUN / STOP", values_[P_RUN] > 0.5f, Color(90, 230, 90));
    Button(840, 382, 110, 42, recording ? "TIE" : "HOLD", (leds & LED_HOLD) != 0, Color(255, 200, 60));
    Button(960, 382, 110, 42, "REST", false, Color(255, 200, 60));
  }

  void DrawSequenceInfo() {
    char buf[96];
    const int len = static_cast<int>(values_[P_OUT_SEQ_LENGTH]);
    Label(845, 448, "SEQUENCER", 12, Color(236, 116, 52), ALIGN_LEFT | ALIGN_MIDDLE);
    std::snprintf(buf, sizeof(buf), "Sequence: %d step%s", len, len == 1 ? "" : "s");
    Label(845, 472, buf, 11, Color(210, 210, 205), ALIGN_LEFT | ALIGN_MIDDLE);
    std::snprintf(buf, sizeof(buf), "Clock: %s", FormatValue(P_TEMPO, values_[P_TEMPO]).c_str());
    Label(845, 492, buf, 11, Color(210, 210, 205), ALIGN_LEFT | ALIGN_MIDDLE);
    fontSize(10);
    fillColor(Color(150, 150, 145));
    textAlign(ALIGN_LEFT | ALIGN_TOP);
    textBox(845, 512, 225,
            "REC, then play notes. HOLD = tie, REST = pause. RUN plays the sequence and the drums; "
            "keys transpose it. Tempo 39 = EXT: follows host tempo and transport.",
            nullptr);
  }

  void DrawDrumGrid() {
    const uint32_t leds = static_cast<uint32_t>(values_[P_OUT_LEDS]);
    const bool override_on = values_[P_DRUM_OVERRIDE] > 0.5f;
    Button(840, 448, 110, 26, "OVERRIDE", override_on, Color(80, 200, 255));
    Button(960, 448, 110, 26, "CLEAR", false, Color(80, 80, 80));
    const int step = static_cast<int>(values_[P_OUT_DRUM_STEP]);
    static const char* names[3] = { "BD", "SD", "HH" };
    static const uint32_t bits[3] = { LED_BD, LED_SD, LED_HH };
    for (int inst = 0; inst < 3; ++inst) {
      const uint16_t pattern = static_cast<uint16_t>(values_[P_OUT_PATTERN_BD + inst]);
      const float cy = 495 + inst * 30;
      DrawLed(848, cy + 11, (leds & bits[inst]) != 0, nullptr);
      Label(856, cy + 11, names[inst], 9, Color(200, 200, 195), ALIGN_LEFT | ALIGN_MIDDLE);
      for (int s = 0; s < 16; ++s) {
        const float cx = 868 + s * 13.2f + 6;
        beginPath();
        roundedRect(cx - 5.5f, cy, 11, 22, 2);
        const bool on = pattern & (1 << s);
        Color col = on ? (override_on ? Color(80, 200, 255) : Color(60, 120, 150))
                       : ((s & 3) == 0 ? Color(70, 70, 76) : Color(52, 52, 58));
        fillColor(col);
        fill();
        if (s == step) {
          strokeColor(Color(255, 255, 255));
          strokeWidth(1.5f);
          stroke();
        }
      }
    }
    Label(845, 590, override_on ? "Playing programmed pattern" : "Generative map (X/Y + density)", 10,
          Color(150, 150, 145), ALIGN_LEFT | ALIGN_MIDDLE);
  }

  void DrawStatus() {
    std::string s = status_;
    if (hover_param_ >= 0 && learning_ < 0) {
      const ParamInfo& p = kParams[hover_param_];
      s = std::string(p.name) + ":  " + FormatValue(hover_param_, values_[hover_param_]);
      if (p.cc >= 0) {
        char buf[48];
        if (p.cc2 >= 0) std::snprintf(buf, sizeof(buf), "     (MIDI CC %d / %d)", p.cc, p.cc2);
        else std::snprintf(buf, sizeof(buf), "     (MIDI CC %d)", p.cc);
        s += buf;
      }
    }
    Label(24, 627, s.c_str(), 11, learning_ >= 0 ? Color(80, 200, 255) : Color(190, 190, 185),
          ALIGN_LEFT | ALIGN_MIDDLE);
  }

  // ------------------------------------------------------------------ state
  float values_[P_COUNT];
  std::vector<Control> controls_;
  int page_ = PAGE_SYNTH;
  float scale_ = 1.0f;
  int hover_param_ = -1;
  int drag_param_ = -1;
  float drag_y_ = 0.0f;
  float drag_norm_ = 0.0f;
  uint last_click_time_ = 0;
  int last_click_param_ = -1;
  int learning_ = -1;
  std::string status_ = "Drag knobs (Shift = fine), double-click to reset, mouse wheel to step.";

  DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnuUI)
};

UI* createUI() { return new AnuUI(); }

END_NAMESPACE_DISTRHO
