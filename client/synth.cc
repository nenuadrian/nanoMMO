#include "client/synth.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace nanommo {

// Seeds next to each other start far apart: MurmurHash3's last steps mix each
// bit of the seed into the others.
Noise::Noise(uint32_t seed) : state_(seed + 0x9E3779B9u) {
  state_ ^= state_ >> 16;
  state_ *= 0x85EBCA6Bu;
  state_ ^= state_ >> 13;
  state_ *= 0xC2B2AE35u;
  state_ ^= state_ >> 16;
}

float Noise::Uniform() {
  // A linear congruential generator, whose top bits are the random ones.
  state_ = state_ * 1664525u + 1013904223u;
  return static_cast<float>(state_ >> 8) / 16777216.0f;
}

float Noise::Between(float low, float high) {
  return low + (high - low) * Uniform();
}

// A state variable filter, as Andrew Simper solved it for a sample at a time:
// it stays steady however fast the frequency moves.
Filter::Out Filter::Run(float in, float hertz, float q) {
  // Up to just under half the rate, the highest it can carry.
  const float limited = std::clamp(hertz, 20.0f, 0.45f * kRate);
  const float g = std::tan(std::numbers::pi_v<float> * limited / kRate);
  const float k = 1.0f / q;
  const float a1 = 1.0f / (1.0f + g * (g + k));
  const float a2 = g * a1;
  const float a3 = g * a2;
  const float v3 = in - sum2_;
  const float band = a1 * sum1_ + a2 * v3;
  const float low = sum2_ + a2 * sum1_ + a3 * v3;
  sum1_ = 2.0f * band - sum1_;
  sum2_ = 2.0f * low - sum2_;
  return {.low = low, .band = band, .high = in - k * band - low};
}

float Oscillator::Next(float hertz) {
  const float sample = std::sin(2.0f * std::numbers::pi_v<float> * phase_);
  phase_ += hertz / kRate;
  phase_ -= std::floor(phase_);
  return sample;
}

float Envelope(float t, float attack, float decay) {
  if (t < 0.0f) {
    return 0.0f;
  }
  if (t < attack) {
    return t / attack;
  }
  return std::exp(-(t - attack) / decay);
}

int SamplesIn(float seconds) {
  return static_cast<int>(std::lround(std::max(seconds, 0.0f) * kRate));
}

void Mix(Samples& mix, const Samples& sound, float at, float gain) {
  const std::size_t start = SamplesIn(at);
  if (mix.size() < start + sound.size()) {
    mix.resize(start + sound.size(), 0.0f);
  }
  for (std::size_t i = 0; i < sound.size(); ++i) {
    mix[start + i] += gain * sound[i];
  }
}

float Peak(const Samples& samples) {
  float peak = 0.0f;
  for (const float sample : samples) {
    peak = std::max(peak, std::abs(sample));
  }
  return peak;
}

void Normalize(Samples& samples, float peak) {
  const float loudest = Peak(samples);
  if (loudest == 0.0f) {
    return;
  }
  for (float& sample : samples) {
    sample *= peak / loudest;
  }
}

void FadeOut(Samples& samples) {
  const std::size_t fade =
      std::min<std::size_t>(SamplesIn(0.01f), samples.size());
  for (std::size_t i = 0; i < fade; ++i) {
    samples[samples.size() - 1 - i] *= static_cast<float>(i) / fade;
  }
}

}  // namespace nanommo
