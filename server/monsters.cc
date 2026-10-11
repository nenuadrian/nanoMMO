#include "server/monsters.h"

#include <algorithm>
#include <cmath>
#include <map>
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
          .experience = 50,
          .damage = 5,
          .swing_time = std::chrono::milliseconds(900),
          .hit_time = std::chrono::milliseconds(650)};
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

uint32_t Spot(const Monster& monster,
              const std::map<uint32_t, Player>& players) {
  uint32_t nearest = 0;
  float closest = kSight;
  for (const auto& [id, player] : players) {
    const float away = std::hypot(player.x - monster.x, player.z - monster.z);
    const float from_home =
        std::hypot(player.x - monster.home_x, player.z - monster.home_z);
    if (player.motion != Motion::kDead && away <= closest &&
        from_home <= kLeash) {
      nearest = id;
      closest = away;
    }
  }
  return nearest;
}

void Hunt(Monster& monster, const std::map<uint32_t, Player>& players,
          std::chrono::nanoseconds time, std::mt19937& random) {
  monster.prey = Spot(monster, players);
  if (monster.prey == 0) {
    if (monster.motion == Motion::kRun) {
      monster.motion = Motion::kIdle;
      monster.rest = RandomRest(random);
    }
    Wander(monster, time, random);
    return;
  }

  const Player& player = players.at(monster.prey);
  const float dx = player.x - monster.x;
  const float dz = player.z - monster.z;
  const float distance = std::hypot(dx, dz);
  monster.yaw = std::atan2(dx, dz);
  if (distance <= kMonsterReach) {
    monster.motion = Motion::kAttack;
    monster.swing = std::chrono::nanoseconds::zero();
    return;
  }
  const float step =
      std::min(World::kRunSpeed * std::chrono::duration<float>(time).count(),
               distance - kMonsterReach);
  monster.x += dx / distance * step;
  monster.z += dz / distance * step;
  monster.motion = Motion::kRun;
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
