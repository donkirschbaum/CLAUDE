// Fixed-point helpers mirroring avrlib/op.h and anu/dsp_utils.h so that the
// ported firmware code behaves bit-exactly like on the ATmega.
// Based on code (c) 2012 Emilie Gillet, GPLv3.
#pragma once
#include <cstdint>

namespace anu {

static inline uint8_t U8Mix(uint8_t a, uint8_t b, uint8_t balance) {
  return static_cast<uint8_t>((a * (255 - balance) + b * balance) >> 8);
}
static inline uint8_t U8U8MulShift8(uint8_t a, uint8_t b) {
  return static_cast<uint8_t>((a * b) >> 8);
}
static inline int8_t S8U8MulShift8(int8_t a, uint8_t b) {
  return static_cast<int8_t>((a * b) >> 8);
}
static inline int16_t S8U8Mul(int8_t a, uint8_t b) {
  return static_cast<int16_t>(a * b);
}
static inline uint16_t U8U8Mul(uint8_t a, uint8_t b) {
  return static_cast<uint16_t>(a * b);
}
static inline uint16_t U16U16MulShift16(uint16_t a, uint16_t b) {
  return static_cast<uint16_t>((static_cast<uint32_t>(a) * b) >> 16);
}
static inline int16_t S16U8MulShift8(int16_t a, uint8_t b) {
  return static_cast<int16_t>((static_cast<int32_t>(a) * b) >> 8);
}
static inline uint16_t U16U8MulShift8(uint16_t a, uint8_t b) {
  return static_cast<uint16_t>((static_cast<uint32_t>(a) * b) >> 8);
}
static inline uint16_t U16ShiftRight4(uint16_t a) { return a >> 4; }
static inline uint8_t U8ShiftLeft4(uint8_t a) { return static_cast<uint8_t>(a << 4); }

static inline uint8_t InterpolateSample(const uint8_t* table, uint16_t phase) {
  return U8Mix(table[phase >> 8], table[(phase >> 8) + 1], phase & 0xff);
}

static inline int16_t Mix(int16_t a, int16_t b, uint16_t balance) {
  if (b > a) {
    return static_cast<int16_t>(a + U16U16MulShift16(static_cast<uint16_t>(b - a), balance));
  } else {
    return static_cast<int16_t>(a - U16U16MulShift16(static_cast<uint16_t>(a - b), balance));
  }
}
static inline uint16_t Mix(uint16_t a, uint16_t b, uint16_t balance) {
  if (b > a) {
    return static_cast<uint16_t>(a + U16U16MulShift16(b - a, balance));
  } else {
    return static_cast<uint16_t>(a - U16U16MulShift16(a - b, balance));
  }
}
static inline uint16_t InterpolateIncreasing(const uint16_t* table, uint16_t phase) {
  uint16_t a = table[phase >> 8];
  uint16_t b = table[(phase >> 8) + 1];
  return static_cast<uint16_t>(a + U16U8MulShift8(static_cast<uint16_t>(b - a), phase & 0xff));
}

// Galois LFSR from avrlib/random.h (one instance per plugin).
class Random {
 public:
  void Seed(uint16_t s) { state_ = s ? s : 0x2161; }
  void Update() { state_ = (state_ >> 1) ^ (-(state_ & 1) & 0xb400); }
  uint16_t state() const { return state_; }
  uint8_t state_msb() const { return static_cast<uint8_t>(state_ >> 8); }
  uint8_t GetByte() { Update(); return state_msb(); }
  uint16_t GetWord() { Update(); return state_; }
 private:
  uint16_t state_ = 0x2161;
};

}  // namespace anu
