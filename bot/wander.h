#ifndef NANOMMO_BOT_WANDER_H_
#define NANOMMO_BOT_WANDER_H_

#include <chrono>
#include <limits>
#include <optional>
#include <random>

#include "protocol/nanommo.pb.h"

namespace nanommo {

protocol::Input RandomDirection(std::mt19937& random);

std::chrono::milliseconds RandomHold(std::mt19937& random);

/// Somewhere a bot heads for, across the island.
struct Trip {
  float to_x = 0.0f;
  float to_z = 0.0f;
  float closest = std::numeric_limits<float>::infinity();
  int stuck = 0;
};

/// A place within 170 metres of the middle, where most of the island is.
Trip RandomTrip(std::mt19937& random);

/// Running towards the end of the trip, from x and z, or nothing once the bot
/// is there, or has stopped getting closer.
std::optional<protocol::Input> Travel(Trip& trip, float x, float z);

}  // namespace nanommo

#endif  // NANOMMO_BOT_WANDER_H_
