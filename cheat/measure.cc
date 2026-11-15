#include "cheat/measure.h"

#include <cmath>
#include <cstddef>

namespace nanommo {

namespace {

// A second of worlds, so that one late tick doesn't swing the speed.
constexpr double kOver = 1.0;
// Five seconds of hits, as a swing takes about one.
constexpr double kHitsOver = 5.0;

}  // namespace

void Speedometer::Add(double sent, float x, float z) {
  if (!places_.empty() && sent <= places_.back().sent) {
    return;
  }
  places_.push_back({.sent = sent, .x = x, .z = z});
  while (places_.size() > 2 && places_.back().sent - places_[1].sent >= kOver) {
    places_.pop_front();
  }
}

float Speedometer::MetresASecond() const {
  if (places_.size() < 2) {
    return 0.0f;
  }
  double metres = 0.0;
  for (std::size_t i = 1; i < places_.size(); ++i) {
    metres += std::hypot(places_[i].x - places_[i - 1].x,
                         places_[i].z - places_[i - 1].z);
  }
  return static_cast<float>(metres /
                            (places_.back().sent - places_.front().sent));
}

void Hits::Add(double at, uint32_t amount) {
  hits_.push_back({.at = at, .amount = amount});
  while (!hits_.empty() && at - hits_.front().at > kHitsOver) {
    hits_.pop_front();
  }
}

float Hits::PerSecond(double now) const {
  std::size_t count = 0;
  for (const Hit& hit : hits_) {
    count += now - hit.at < kHitsOver ? 1 : 0;
  }
  return static_cast<float>(static_cast<double>(count) / kHitsOver);
}

float Hits::DamagePerSecond(double now) const {
  uint32_t damage = 0;
  for (const Hit& hit : hits_) {
    damage += now - hit.at < kHitsOver ? hit.amount : 0;
  }
  return static_cast<float>(damage / kHitsOver);
}

}  // namespace nanommo
