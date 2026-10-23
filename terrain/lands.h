#ifndef NANOMMO_TERRAIN_LANDS_H_
#define NANOMMO_TERRAIN_LANDS_H_

#include <array>
#include <cstdint>
#include <string_view>

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
};

/// Each world has these lands. The server tells a client which one it's in by
/// its place in the list, so the server and the client list them in one order.
constexpr std::array<Land, 2> kLands = {{
    {.name = "Meadows", .seed = 0, .wading = Terrain::kWading},
    {.name = "Ashlands", .seed = 1000, .wading = 0.0f},
}};

/// A ring of stones in a land, which takes players to another.
struct Gate {
  /// Where it stands, and the land it takes you to, by their places in kLands.
  int land = 0;
  Place place;
  int to = 0;
};

constexpr std::array<Gate, 2> kGates = {{
    // Between the Meadows and the Ashlands.
    {.land = 0, .place = {0.0f, -10.0f}, .to = 1},
    {.land = 1, .place = {0.0f, -10.0f}, .to = 0},
}};

/// Each gate has a twin in the land it takes you to, which leads back.
constexpr bool GatesLeadBack() {
  for (const Gate& gate : kGates) {
    bool twin = false;
    for (const Gate& other : kGates) {
      twin = twin || (other.land == gate.to && other.to == gate.land);
    }
    if (!twin) {
      return false;
    }
  }
  return true;
}
static_assert(GatesLeadBack(), "each gate needs a twin");

/// In metres from the middle of a gate.
constexpr float kGateRadius = 1.0f;

/// Past the edge of a gate, so that players don't arrive inside it.
constexpr float kArrival = 3.0f;

/// Its place in the list, or the first land's for a name that isn't there.
int LandNamed(std::string_view name);

bool IsLand(std::string_view name);

bool InGate(const Gate& gate, float x, float z);

/// The gate in the land this one takes you to, which leads back.
const Gate& Twin(const Gate& gate);

/// Where a player who goes through the gate comes out: past the middle of its
/// twin, the way they were facing, so that they walk on out of it.
Place Arrival(const Gate& gate, float yaw);

}  // namespace nanommo

#endif  // NANOMMO_TERRAIN_LANDS_H_
