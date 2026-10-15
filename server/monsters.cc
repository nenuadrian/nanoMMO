#include "server/monsters.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numbers>
#include <utility>
#include <vector>

namespace nanommo {

namespace {

constexpr float kCampsFromTheMiddle = 30.0f;

// Above the beach, and below the rock.
constexpr float kLowestCamp = 1.0f;
constexpr float kHighestCamp = 10.0f;

// Where each camp's three monsters stand, from the middle of the camp.
constexpr std::array<std::pair<float, float>, 3> kCampLayout = {{
    {-1.5f, 0.0f},
    {1.5f, 0.5f},
    {0.0f, -1.5f},
}};

// The square root spreads the places evenly, not bunched up around home.
void PickTarget(Monster& monster, const Terrain& terrain,
                std::mt19937& random) {
  std::uniform_real_distribution<float> unit(0.0f, 1.0f);
  const float angle = 2.0f * std::numbers::pi_v<float> * unit(random);
  const float distance = kWanderRadius * std::sqrt(unit(random));
  monster.target_x = std::clamp(monster.home_x + distance * std::sin(angle),
                                -terrain.edge(), terrain.edge());
  monster.target_z = std::clamp(monster.home_z + distance * std::cos(angle),
                                -terrain.edge(), terrain.edge());
}

}  // namespace

Monster MonsterOf(const MonsterKind& kind, float x, float z) {
  return {.name = kind.name,
          .x = x,
          .z = z,
          .level = kind.level,
          .max_health = kind.max_health,
          .experience = kind.experience,
          .damage = kind.damage,
          .swing_time = kind.swing_time,
          .hit_time = kind.hit_time};
}

void SpawnMonsters(World& land, int index) {
  for (const MonsterSpawn& spawn : kMonsterSpawns) {
    if (spawn.land == index) {
      land.Spawn(MonsterOf(*spawn.kind, spawn.x, spawn.z));
    }
  }
  const Terrain& terrain = land.terrain();
  for (const MonsterCamps& camps : kMonsterCamps) {
    if (camps.land != index) {
      continue;
    }
    for (const auto& [camp_x, camp_z] : Camps(terrain, camps.count)) {
      for (const auto& [x, z] : kCampLayout) {
        if (terrain.Walkable(camp_x + x, camp_z + z)) {
          land.Spawn(MonsterOf(*camps.kind, camp_x + x, camp_z + z));
        }
      }
    }
  }
}

std::vector<std::pair<float, float>> Camps(const Terrain& terrain, int count) {
  std::mt19937 random(terrain.seed());
  std::uniform_real_distribution<float> place(-terrain.edge(), terrain.edge());
  std::vector<std::pair<float, float>> camps;
  // Most of the grid is sea, so it takes a few tries to find each one.
  for (int tries = 0; tries < 1000 && std::ssize(camps) < count; ++tries) {
    const float x = place(random);
    const float z = place(random);
    const float height = terrain.Height(x, z);
    if (std::hypot(x, z) >= kCampsFromTheMiddle && height >= kLowestCamp &&
        height <= kHighestCamp && terrain.Walkable(x, z)) {
      camps.emplace_back(x, z);
    }
  }
  return camps;
}

std::chrono::nanoseconds RandomRest(std::mt19937& random) {
  std::uniform_int_distribution<int> ms(kShortestRest.count(),
                                        kLongestRest.count());
  return std::chrono::milliseconds(ms(random));
}

void Wander(Monster& monster, const Terrain& terrain,
            std::chrono::nanoseconds time, std::mt19937& random) {
  if (monster.motion == Motion::kIdle) {
    monster.rest -= time;
    if (monster.rest > std::chrono::nanoseconds::zero()) {
      return;
    }
    PickTarget(monster, terrain, random);
    monster.motion = Motion::kWalk;
  }

  const float dx = monster.target_x - monster.x;
  const float dz = monster.target_z - monster.z;
  const float distance = std::hypot(dx, dz);
  const float step =
      World::kWalkSpeed * std::chrono::duration<float>(time).count();
  const float x = monster.x;
  const float z = monster.z;
  if (distance <= step) {
    Walk(terrain, monster.x, monster.z, monster.target_x, monster.target_z);
  } else {
    Walk(terrain, monster.x, monster.z, x + dx / distance * step,
         z + dz / distance * step);
    monster.yaw = std::atan2(dx, dz);
  }
  // There, or stuck at the sea, it rests before it picks another place.
  if (distance <= step || (monster.x == x && monster.z == z)) {
    monster.motion = Motion::kIdle;
    monster.rest = RandomRest(random);
  }
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
          const Terrain& terrain, std::chrono::nanoseconds time,
          std::mt19937& random) {
  monster.prey = Spot(monster, players);
  if (monster.prey == 0) {
    if (monster.motion == Motion::kRun) {
      monster.motion = Motion::kIdle;
      monster.rest = RandomRest(random);
    }
    Wander(monster, terrain, time, random);
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
  Walk(terrain, monster.x, monster.z, monster.x + dx / distance * step,
       monster.z + dz / distance * step);
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
