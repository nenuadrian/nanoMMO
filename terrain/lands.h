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
  /// Whether each player who goes in gets a copy of the land of their own,
  /// rather than the one copy that everyone in the world shares.
  bool dungeon = false;
};

/// Each world has these lands. The server tells a client which one it's in by
/// its place in the list, so the server and the client list them in one order.
constexpr std::array<Land, 3> kLands = {{
    {.name = "Meadows", .seed = 0, .wading = Terrain::kWading},
    {.name = "Ashlands", .seed = 1000, .wading = 0.0f},
    {.name = "Crypt", .seed = 2000, .wading = 0.0f, .dungeon = true},
}};

/// A ring of stones in a land, which takes players to another.
struct Gate {
  /// Where it stands, and the land it takes you to, by their places in kLands.
  int land = 0;
  Place place;
  int to = 0;
};

constexpr std::array<Gate, 4> kGates = {{
    // Between the Meadows and the Ashlands.
    {.land = 0, .place = {0.0f, -10.0f}, .to = 1},
    {.land = 1, .place = {0.0f, -10.0f}, .to = 0},
    // Between the Meadows and the Crypt.
    {.land = 0, .place = {12.0f, 0.0f}, .to = 2},
    {.land = 2, .place = {0.0f, -10.0f}, .to = 0},
}};

/// Each gate has a twin in the land it takes you to, which leads back, and
/// each dungeon has a gate, which is the way out.
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
  for (int land = 0; land < static_cast<int>(kLands.size()); ++land) {
    bool gate = false;
    for (const Gate& other : kGates) {
      gate = gate || other.land == land;
    }
    if (kLands[land].dungeon && !gate) {
      return false;
    }
  }
  return true;
}
static_assert(GatesLeadBack(),
              "each gate needs a twin, and each dungeon a way out");

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

/// The dungeon's way out: its first gate.
const Gate& WayOut(int dungeon);

/// Where a player who goes through the gate comes out: past the middle of its
/// twin, the way they were facing, so that they walk on out of it.
Place Arrival(const Gate& gate, float yaw);

}  // namespace nanommo

#endif  // NANOMMO_TERRAIN_LANDS_H_
