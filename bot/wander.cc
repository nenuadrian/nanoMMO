#include "bot/wander.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>
#include <vector>

namespace nanommo {

namespace {

constexpr float kReach = 170.0f;
constexpr int kStuck = 3;
constexpr float kThroughTheGate = 0.2f;

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

Place RandomGate(std::mt19937& random, int land) {
  std::vector<Place> gates;
  for (const Gate& gate : kGates) {
    if (gate.land == land) {
      gates.push_back(gate.place);
    }
  }
  // Each land has a gate, as the way back to the land before.
  return gates[std::uniform_int_distribution<std::size_t>(
      0, gates.size() - 1)(random)];
}

Trip RandomTrip(std::mt19937& random, const Place& gate) {
  std::uniform_real_distribution<float> unit(0.0f, 1.0f);
  if (unit(random) < kThroughTheGate) {
    // All the way to its middle: three metres short, it would stop outside.
    return {.to_x = gate.x, .to_z = gate.z, .there = kGateRadius / 2};
  }
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
  if (distance < trip.there) {
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

std::optional<protocol::Input> StepOut(const MonsterTable& table,
                                       const protocol::World& world, float x,
                                       float z) {
  for (const protocol::Monster& monster : world.monsters()) {
    const auto kind = table.kinds.find(monster.kind());
    if (monster.motion() != protocol::MOTION_SKILL ||
        kind == table.kinds.end()) {
      continue;
    }
    const auto skill = std::ranges::find(kind->second.skills(), monster.skill(),
                                         &monsters::Skill::id);
    if (skill == kind->second.skills().end()) {
      continue;
    }
    monsters::Skill clear = *skill;
    clear.set_size(skill->size() + kClear);
    if (!InShape(clear, {.x = monster.x(), .z = monster.z()}, monster.yaw(),
                 {.x = x, .z = z})) {
      continue;
    }
    // Away from the monster: out of a circle, and out of the far end of a
    // cone, unless its nearer side is nearer, as a narrow one's is.
    const float dx = x - monster.x();
    const float dz = z - monster.z();
    const float away = std::hypot(dx, dz);
    float out_x = away > 0.0f ? dx / away : 1.0f;
    float out_z = away > 0.0f ? dz / away : 0.0f;
    if (skill->shape() == monsters::CONE && skill->arc() < 180.0f) {
      // Along the way it's aimed, and across it, to the side the bot's on.
      const float ahead_x = std::sin(monster.yaw());
      const float ahead_z = std::cos(monster.yaw());
      const float along = dx * ahead_x + dz * ahead_z;
      const float across = dx * ahead_z - dz * ahead_x;
      const float half =
          skill->arc() / 2.0f * std::numbers::pi_v<float> / 180.0f;
      const float to_side = along * std::tan(half) - std::abs(across);
      if (to_side < skill->size() - away) {
        const float side = across >= 0.0f ? 1.0f : -1.0f;
        out_x = side * ahead_z;
        out_z = -side * ahead_x;
      }
    }
    protocol::Input input;
    input.set_x(out_x);
    input.set_z(out_z);
    input.set_run(true);
    return input;
  }
  return std::nullopt;
}

}  // namespace nanommo
