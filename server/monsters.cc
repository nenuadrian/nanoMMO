#include "server/monsters.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace nanommo {

namespace {

// The square root spreads the places evenly, not bunched up around home.
void PickTarget(Monster& monster, std::mt19937& random) {
  std::uniform_real_distribution<float> unit(0.0f, 1.0f);
  const float angle = 2.0f * std::numbers::pi_v<float> * unit(random);
  const float distance = kWanderRadius * std::sqrt(unit(random));
  monster.target_x = std::clamp(monster.home_x + distance * std::sin(angle),
                                -World::kEdge, World::kEdge);
  monster.target_z = std::clamp(monster.home_z + distance * std::cos(angle),
                                -World::kEdge, World::kEdge);
}

}  // namespace

Monster Skeleton(float x, float z) {
  return {.name = "Skeleton",
          .x = x,
          .z = z,
          .level = 1,
          .max_health = 50,
          .experience = 50};
}

std::chrono::nanoseconds RandomRest(std::mt19937& random) {
  std::uniform_int_distribution<int> ms(kShortestRest.count(),
                                        kLongestRest.count());
  return std::chrono::milliseconds(ms(random));
}

void Wander(Monster& monster, std::chrono::nanoseconds time,
            std::mt19937& random) {
  if (monster.motion == Motion::kIdle) {
    monster.rest -= time;
    if (monster.rest > std::chrono::nanoseconds::zero()) {
      return;
    }
    PickTarget(monster, random);
    monster.motion = Motion::kWalk;
  }

  const float dx = monster.target_x - monster.x;
  const float dz = monster.target_z - monster.z;
  const float distance = std::hypot(dx, dz);
  const float step =
      World::kWalkSpeed * std::chrono::duration<float>(time).count();
  if (distance <= step) {
    monster.x = monster.target_x;
    monster.z = monster.target_z;
    monster.motion = Motion::kIdle;
    monster.rest = RandomRest(random);
    return;
  }
  monster.x += dx / distance * step;
  monster.z += dz / distance * step;
  monster.yaw = std::atan2(dx, dz);
}

void Revive(Monster& monster, std::chrono::nanoseconds time,
            std::mt19937& random) {
  monster.respawn -= time;
  if (monster.respawn > std::chrono::nanoseconds::zero()) {
    return;
  }
  monster.x = monster.home_x;
  monster.z = monster.home_z;
  monster.health = monster.max_health;
  monster.motion = Motion::kIdle;
  monster.rest = RandomRest(random);
}

}  // namespace nanommo
