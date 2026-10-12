#include "terrain/terrain.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "terrain/noise.h"

namespace nanommo {

namespace {

constexpr int kBoardCells = 6;
constexpr int kIslandCells = 256;

float Smooth(float t) {
  const float clamped = std::clamp(t, 0.0f, 1.0f);
  return clamped * clamped * (3.0f - 2.0f * clamped);
}

float IslandHeight(uint32_t seed, float x, float z, float half_size) {
  const float from_middle = std::sqrt(x * x + z * z);
  const float hills = Fractal(seed, x / 90.0f, z / 90.0f, 5);
  const float peaks =
      std::max(Fractal(seed + 101, x / 140.0f, z / 140.0f, 4), 0.0f);
  float height = 3.0f + 7.0f * hills + 60.0f * peaks * peaks;
  // The land sinks into the sea towards the edge.
  const float coast = Smooth((from_middle / half_size - 0.55f) / 0.4f);
  height = height * (1.0f - coast) - 12.0f * coast;
  // Gentle slopes in the middle, where the players start, and never under the
  // sea.
  const float meadow = Smooth((from_middle - 12.0f) / 24.0f);
  return 2.0f + (height - 2.0f) * (0.15f + 0.85f * meadow);
}

}  // namespace

Terrain::Terrain()
    : cells_(kBoardCells), corners_((kBoardCells + 1) * (kBoardCells + 1)) {}

Terrain::Terrain(uint32_t seed)
    : seed_(seed),
      cells_(kIslandCells),
      corners_((kIslandCells + 1) * (kIslandCells + 1)) {
  for (int j = 0; j <= cells_; ++j) {
    for (int i = 0; i <= cells_; ++i) {
      const float x = i * kCell - half_size();
      const float z = j * kCell - half_size();
      corners_[j * (cells_ + 1) + i] = IslandHeight(seed, x, z, half_size());
    }
  }
}

float Terrain::Height(float x, float z) const {
  const float across = (x + half_size()) / kCell;
  const float down = (z + half_size()) / kCell;
  const int i = std::clamp(static_cast<int>(std::floor(across)), 0, cells_ - 1);
  const int j = std::clamp(static_cast<int>(std::floor(down)), 0, cells_ - 1);
  const float u = std::clamp(across - i, 0.0f, 1.0f);
  const float v = std::clamp(down - j, 0.0f, 1.0f);
  const float top_left = Corner(i, j);
  const float top_right = Corner(i + 1, j);
  const float bottom_left = Corner(i, j + 1);
  const float bottom_right = Corner(i + 1, j + 1);
  // Each cell's two triangles meet on the diagonal from top left to bottom
  // right.
  if (u >= v) {
    return top_left + (top_right - top_left) * u +
           (bottom_right - top_right) * v;
  }
  return top_left + (bottom_right - bottom_left) * u +
         (bottom_left - top_left) * v;
}

bool Terrain::Walkable(float x, float z) const {
  return std::abs(x) <= edge() && std::abs(z) <= edge() &&
         Height(x, z) >= kSeaLevel - kWading;
}

}  // namespace nanommo
