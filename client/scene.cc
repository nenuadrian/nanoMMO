#include "client/scene.h"

#include <vector>

namespace nanommo {

namespace {

constexpr int kTiles = 12;
constexpr Color kLightTile{84, 92, 102, 255};
constexpr Color kDarkTile{66, 71, 82, 255};

}  // namespace

std::vector<Cube> Ground() {
  std::vector<Cube> cubes;
  for (int z = 0; z < kTiles; ++z) {
    for (int x = 0; x < kTiles; ++x) {
      // A tile is a flat box, a fifth of a unit thick, with its top at zero.
      const Vector3 center{x - kTiles / 2 + 0.5f, -0.1f, z - kTiles / 2 + 0.5f};
      const bool light = (x + z) % 2 == 0;
      cubes.push_back({.position = center,
                       .size = {1.0f, 0.2f, 1.0f},
                       .color = light ? kLightTile : kDarkTile});
    }
  }
  return cubes;
}

}  // namespace nanommo
