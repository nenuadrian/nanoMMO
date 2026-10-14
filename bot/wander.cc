#include "bot/wander.h"

#include <cmath>
#include <numbers>
#include <optional>

namespace nanommo {

namespace {

constexpr float kReach = 170.0f;
constexpr float kThere = 3.0f;
constexpr int kStuck = 3;

}  // namespace

protocol::Input RandomDirection(std::mt19937& random) {
  std::uniform_int_distribution<int> key(-1, 1);
  protocol::Input input;
  input.set_x(static_cast<float>(key(random)));
  input.set_z(static_cast<float>(key(random)));
  std::bernoulli_distribution shift(1.0 / 3.0);
  input.set_run(shift(random));
  return input;
}

std::chrono::milliseconds RandomHold(std::mt19937& random) {
  std::uniform_int_distribution<int> ms(500, 3000);
  return std::chrono::milliseconds(ms(random));
}

Trip RandomTrip(std::mt19937& random) {
  std::uniform_real_distribution<float> unit(0.0f, 1.0f);
  const float angle = 2.0f * std::numbers::pi_v<float> * unit(random);
  // The square root spreads the trips evenly over the circle's area.
  const float distance = kReach * std::sqrt(unit(random));
  return {.to_x = distance * std::cos(angle),
          .to_z = distance * std::sin(angle)};
}

std::optional<protocol::Input> Travel(Trip& trip, float x, float z) {
  const float dx = trip.to_x - x;
  const float dz = trip.to_z - z;
  const float distance = std::hypot(dx, dz);
  if (distance < kThere) {
    return std::nullopt;
  }
  if (distance < trip.closest - 0.5f) {
    trip.closest = distance;
    trip.stuck = 0;
  } else if (++trip.stuck >= kStuck) {
    return std::nullopt;
  }
  protocol::Input input;
  input.set_x(dx / distance);
  input.set_z(dz / distance);
  input.set_run(true);
  return input;
}

}  // namespace nanommo
