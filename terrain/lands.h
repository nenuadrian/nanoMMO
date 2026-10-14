#ifndef NANOMMO_TERRAIN_LANDS_H_
#define NANOMMO_TERRAIN_LANDS_H_

#include <array>
#include <cstdint>

#include "terrain/terrain.h"

namespace nanommo {

/// In metres from the middle of a land.
struct Place {
  float x = 0.0f;
  float z = 0.0f;
};

struct Land {
  const char* name;
  /// Added to the world's seed, so that each land has an island of its own.
  uint32_t seed = 0;
  /// How deep into its sea players can wade, in metres.
  float wading = Terrain::kWading;
  /// Where the land's gate stands, and the land it takes you to.
  Place gate;
  int to = 0;
};

/// Each world has these lands. The server tells a client which one it's in by
/// its place in the list, so the server and the client list them in one order.
constexpr std::array<Land, 2> kLands = {{
    {.name = "Meadows",
     .seed = 0,
     .wading = Terrain::kWading,
     .gate = {0.0f, -10.0f},
     .to = 1},
    {.name = "Ashlands",
     .seed = 1000,
     .wading = 0.0f,
     .gate = {0.0f, -10.0f},
     .to = 0},
}};

/// In metres from the middle of a gate.
constexpr float kGateRadius = 1.0f;

/// Past the edge of a gate, so that players don't arrive inside it.
constexpr float kArrival = 3.0f;

bool InGate(const Land& land, float x, float z);

/// Where a player who goes through a gate comes out at the land's gate: past
/// its middle, the way they were facing, so that they walk on out of it.
Place Arrival(const Land& land, float yaw);

}  // namespace nanommo

#endif  // NANOMMO_TERRAIN_LANDS_H_
