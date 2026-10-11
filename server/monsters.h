#ifndef NANOMMO_SERVER_MONSTERS_H_
#define NANOMMO_SERVER_MONSTERS_H_

#include <chrono>
#include <cstdint>
#include <map>
#include <random>

#include "server/world.h"

namespace nanommo {

constexpr float kWanderRadius = 3.0f;

constexpr std::chrono::milliseconds kShortestRest{2000};
constexpr std::chrono::milliseconds kLongestRest{6000};

constexpr float kSight = 3.0f;

/// Each skeleton's home is further than this from the middle of the board,
/// so players are safe where they start and come back to life.
constexpr float kLeash = 4.0f;

constexpr float kMonsterReach = 1.2f;

Monster Skeleton(float x, float z);

std::chrono::nanoseconds RandomRest(std::mt19937& random);

void Wander(Monster& monster, std::chrono::nanoseconds time,
            std::mt19937& random);

uint32_t Spot(const Monster& monster,
              const std::map<uint32_t, Player>& players);

void Hunt(Monster& monster, const std::map<uint32_t, Player>& players,
          std::chrono::nanoseconds time, std::mt19937& random);

void Revive(Monster& monster, std::chrono::nanoseconds time,
            std::mt19937& random);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_MONSTERS_H_
