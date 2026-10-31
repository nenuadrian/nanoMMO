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

constexpr Vector3 kStone{0.6f, 3.0f, 0.6f};

// Under each stone, a flat block a little wider, sunk into the ground a bit,
// so that it sits on a slope too.
constexpr Vector3 kBase{0.95f, 0.3f, 0.95f};
constexpr float kSunk = 0.1f;

// The platform: a rim of stone, and the sky's colour inside it, a little
// higher, so that it shows.
constexpr float kRim = 0.3f;
constexpr float kPlatform = 0.3f;
constexpr float kInside = 0.36f;

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
      const float ground = terrain.Height(x, z);
      cubes.push_back({.position = {x, ground + kBase.y / 2 - kSunk, z},
                       .size = kBase,
                       .color = kLooks[land].rock});
      cubes.push_back({.position = {x, ground + kStone.y / 2, z},
                       .size = kStone,
                       .color = kLooks[land].rock,
                       .shape = Shape::kSpire});
    }
    const float ground = terrain.Height(place.x, place.z);
    const float across = 2 * (kGateRadius + kRim);
    cubes.push_back({.position = {place.x, ground, place.z},
                     .size = {across, kPlatform, across},
                     .color = kLooks[land].rock,
                     .shape = Shape::kDisc});
    const std::array<float, 3>& sky = kLooks[gate.to].atmosphere.sky;
    cubes.push_back(
        {.position = {place.x, ground, place.z},
         .size = {2 * kGateRadius, kInside, 2 * kGateRadius},
         .color = ColorFromNormalized({sky[0], sky[1], sky[2], 1.0f}),
         .shape = Shape::kDisc});
  }
  return cubes;
}

}  // namespace nanommo
