#ifndef NANOMMO_SERVER_MONSTERS_H_
#define NANOMMO_SERVER_MONSTERS_H_

#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <random>
#include <utility>
#include <vector>

#include "server/world.h"
#include "terrain/terrain.h"

namespace nanommo {

/// What every monster of a kind has in common.
struct MonsterKind {
  const char* name;
  int level = 1;
  int max_health = 0;
  /// What a player gets for killing one.
  uint32_t experience = 0;
  int damage = 0;
  std::chrono::milliseconds swing_time{0};
  /// How far into its swing the blow lands.
  std::chrono::milliseconds hit_time{0};
};

constexpr MonsterKind kSkeleton = {
    .name = "Skeleton",
    .level = 1,
    .max_health = 50,
    .experience = 50,
    .damage = 5,
    .swing_time = std::chrono::milliseconds(900),
    .hit_time = std::chrono::milliseconds(650),
};

/// A monster that always has its home in the same place, in metres from the
/// middle of its land.
struct MonsterSpawn {
  const MonsterKind* kind;
  /// By its place in kLands.
  int land = 0;
  float x = 0.0f;
  float z = 0.0f;
};

constexpr std::array<MonsterSpawn, 6> kMonsterSpawns = {{
    {.kind = &kSkeleton, .land = 0, .x = -3.5f, .z = -3.0f},
    {.kind = &kSkeleton, .land = 0, .x = 3.5f, .z = -2.5f},
    {.kind = &kSkeleton, .land = 0, .x = 3.0f, .z = 3.5f},
    {.kind = &kSkeleton, .land = 1, .x = -3.5f, .z = -3.0f},
    {.kind = &kSkeleton, .land = 1, .x = 3.5f, .z = -2.5f},
    {.kind = &kSkeleton, .land = 1, .x = 3.0f, .z = 3.5f},
}};

/// Camps of monsters of a kind, in places the land's seed picks (see Camps).
struct MonsterCamps {
  const MonsterKind* kind;
  int land = 0;
  int count = 0;
};

constexpr std::array<MonsterCamps, 2> kMonsterCamps = {{
    {.kind = &kSkeleton, .land = 0, .count = 8},
    {.kind = &kSkeleton, .land = 1, .count = 8},
}};

constexpr float kWanderRadius = 3.0f;

constexpr std::chrono::milliseconds kShortestRest{2000};
constexpr std::chrono::milliseconds kLongestRest{6000};

constexpr float kSight = 3.0f;

/// Each skeleton's home is further than this from the middle of the board,
/// so players are safe where they start and come back to life.
constexpr float kLeash = 4.0f;

constexpr float kMonsterReach = 1.2f;

Monster MonsterOf(const MonsterKind& kind, float x, float z);

/// Spawns the land's monsters from kMonsterSpawns and kMonsterCamps. The index
/// is the land's place in kLands.
void SpawnMonsters(World& land, int index);

/// Places for camps of monsters, which the terrain's seed picks: between the
/// beach and the rock, and well away from the middle, where the players start.
std::vector<std::pair<float, float>> Camps(const Terrain& terrain, int count);

std::chrono::nanoseconds RandomRest(std::mt19937& random);

void Wander(Monster& monster, const Terrain& terrain,
            std::chrono::nanoseconds time, std::mt19937& random);

uint32_t Spot(const Monster& monster,
              const std::map<uint32_t, Player>& players);

void Hunt(Monster& monster, const std::map<uint32_t, Player>& players,
          const Terrain& terrain, std::chrono::nanoseconds time,
          std::mt19937& random);

void Revive(Monster& monster, std::chrono::nanoseconds time,
            std::mt19937& random);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_MONSTERS_H_
