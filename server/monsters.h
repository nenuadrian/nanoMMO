#ifndef NANOMMO_SERVER_MONSTERS_H_
#define NANOMMO_SERVER_MONSTERS_H_

#include <chrono>
#include <cstdint>
#include <map>
#include <random>
#include <utility>
#include <vector>

#include "content/monsters.h"
#include "server/world.h"
#include "terrain/terrain.h"

namespace nanommo {

constexpr std::chrono::milliseconds kShortestRest{2000};
constexpr std::chrono::milliseconds kLongestRest{6000};

Monster MonsterOf(const monsters::Kind& kind, float x, float z);

/// Spawns the land's monsters from the table's spawns and camps. The index is
/// the land's place in kLands.
void SpawnMonsters(World& land, int index, const MonsterTable& table);

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
