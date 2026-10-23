#ifndef NANOMMO_BOT_WANDER_H_
#define NANOMMO_BOT_WANDER_H_

#include <chrono>
#include <limits>
#include <optional>
#include <random>

#include "protocol/nanommo.pb.h"
#include "terrain/lands.h"

namespace nanommo {

protocol::Input RandomDirection(std::mt19937& random);

std::chrono::milliseconds RandomHold(std::mt19937& random);

/// Somewhere a bot heads for, across the island.
struct Trip {
  float to_x = 0.0f;
  float to_z = 0.0f;
  float closest = std::numeric_limits<float>::infinity();
  int stuck = 0;
  /// How close to the end counts as there, in metres.
  float there = 3.0f;
};

/// One of the land's gates, at random, as players go through each of them.
Place RandomGate(std::mt19937& random, int land);

/// A place within 170 metres of the middle, where most of the island is, or
/// on one trip in five, the middle of the gate, which takes the bot to
/// another land, as players go there too.
Trip RandomTrip(std::mt19937& random, const Place& gate);

/// Running towards the end of the trip, from x and z, or nothing once the bot
/// is there, or has stopped getting closer.
std::optional<protocol::Input> Travel(Trip& trip, float x, float z);

}  // namespace nanommo

#endif  // NANOMMO_BOT_WANDER_H_
