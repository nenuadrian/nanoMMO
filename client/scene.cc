#include "client/scene.h"

#include <array>
#include <vector>

namespace nanommo {

namespace {

constexpr int kTiles = 12;
constexpr Color kLightTile{84, 92, 102, 255};
constexpr Color kDarkTile{66, 71, 82, 255};

constexpr std::array<Color, 6> kPlayerColors = {{
    {242, 140, 51, 255},
    {64, 153, 242, 255},
    {89, 204, 102, 255},
    {178, 115, 242, 255},
    {242, 89, 102, 255},
    {242, 217, 77, 255},
}};
constexpr Color kMarker{255, 255, 255, 255};

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

std::vector<Cube> Scene(const protocol::World& world) {
  std::vector<Cube> cubes = Ground();
  for (const protocol::Player& player : world.players()) {
    const Vector3 position{player.x(), 0.5f, player.z()};
    cubes.push_back(
        {.position = position,
         .color = kPlayerColors[(player.id() - 1) % kPlayerColors.size()]});
    if (player.id() == world.you()) {
      cubes.push_back({.position = {position.x, position.y + 1.0f, position.z},
                       .size = {0.25f, 0.25f, 0.25f},
                       .color = kMarker});
    }
  }
  return cubes;
}

}  // namespace nanommo
