#ifndef NANOMMO_SERVER_MONSTERS_H_
#define NANOMMO_SERVER_MONSTERS_H_

#include <chrono>
#include <random>

#include "server/world.h"

namespace nanommo {

constexpr float kWanderRadius = 3.0f;

constexpr std::chrono::milliseconds kShortestRest{2000};
constexpr std::chrono::milliseconds kLongestRest{6000};

Monster Skeleton(float x, float z);

std::chrono::nanoseconds RandomRest(std::mt19937& random);

void Wander(Monster& monster, std::chrono::nanoseconds time,
            std::mt19937& random);

void Revive(Monster& monster, std::chrono::nanoseconds time,
            std::mt19937& random);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_MONSTERS_H_
