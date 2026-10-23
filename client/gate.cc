#include "client/gate.h"

#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include "client/atmosphere.h"
#include "terrain/lands.h"

namespace nanommo {

namespace {

constexpr int kStones = 6;

// From the middle of the gate.
constexpr float kRing = 2.2f;

constexpr Vector3 kStone{0.6f, 2.6f, 0.6f};

}  // namespace

std::vector<Cube> GateCubes(const Terrain& terrain, int land) {
  std::vector<Cube> cubes;
  for (const Gate& gate : kGates) {
    if (gate.land != land) {
      continue;
    }
    const Place& place = gate.place;
    for (int i = 0; i < kStones; ++i) {
      const float angle = 2.0f * std::numbers::pi_v<float> * i / kStones;
      const float x = place.x + kRing * std::cos(angle);
      const float z = place.z + kRing * std::sin(angle);
      // A cube's position is its middle, so it goes up by half its height.
      cubes.push_back({.position = {x, terrain.Height(x, z) + kStone.y / 2, z},
                       .size = kStone,
                       .color = kLooks[land].rock});
    }
    const std::array<float, 3>& sky = kLooks[gate.to].atmosphere.sky;
    cubes.push_back(
        {.position = {place.x, terrain.Height(place.x, place.z), place.z},
         .size = {2 * kGateRadius, 0.2f, 2 * kGateRadius},
         .color = ColorFromNormalized({sky[0], sky[1], sky[2], 1.0f})});
  }
  return cubes;
}

}  // namespace nanommo
