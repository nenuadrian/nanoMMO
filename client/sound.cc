#include "client/sound.h"

#include <cmath>

namespace nanommo {

namespace {

constexpr float kRangeDecibels = 40.0f;

}  // namespace

float Gain(float slider) {
  if (slider <= 0.0f) {
    return 0.0f;
  }
  // Twenty decibels are ten times the amplitude.
  return std::pow(10.0f, (slider - 1.0f) * kRangeDecibels / 20.0f);
}

}  // namespace nanommo
