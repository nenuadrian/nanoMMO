#include "terrain/noise.h"

#include <cmath>
#include <cstdint>

namespace nanommo {

namespace {

float Value(uint32_t seed, int32_t x, int32_t z) {
  // The top 24 bits, as many as a float holds exactly.
  return static_cast<float>(Hash(seed, x, z) >> 8) * (2.0f / 16777215.0f) -
         1.0f;
}

float Smooth(float t) { return t * t * (3.0f - 2.0f * t); }

float Mix(float a, float b, float t) { return a + (b - a) * t; }

}  // namespace

uint32_t Hash(uint32_t seed, int32_t x, int32_t z) {
  uint32_t hash = seed ^ (static_cast<uint32_t>(x) * 0x9E3779B1u) ^
                  (static_cast<uint32_t>(z) * 0x85EBCA77u);
  // MurmurHash3's last step, which mixes each bit into all the others.
  hash ^= hash >> 16;
  hash *= 0x85EBCA6Bu;
  hash ^= hash >> 13;
  hash *= 0xC2B2AE35u;
  hash ^= hash >> 16;
  return hash;
}

float Noise(uint32_t seed, float x, float z) {
  const float left = std::floor(x);
  const float top = std::floor(z);
  const auto i = static_cast<int32_t>(left);
  const auto j = static_cast<int32_t>(top);
  const float across = Smooth(x - left);
  const float down = Smooth(z - top);
  const float upper = Mix(Value(seed, i, j), Value(seed, i + 1, j), across);
  const float lower =
      Mix(Value(seed, i, j + 1), Value(seed, i + 1, j + 1), across);
  return Mix(upper, lower, down);
}

float Fractal(uint32_t seed, float x, float z, int octaves) {
  float sum = 0.0f;
  float total = 0.0f;
  float strength = 1.0f;
  float scale = 1.0f;
  for (int octave = 0; octave < octaves; ++octave) {
    sum += strength * Noise(seed + octave, x * scale, z * scale);
    total += strength;
    strength *= 0.5f;
    scale *= 2.0f;
  }
  return sum / total;
}

}  // namespace nanommo
