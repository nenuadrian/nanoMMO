#include "terrain/terrain.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "terrain/noise.h"

namespace nanommo {

namespace {

constexpr int kBoardCells = 6;
constexpr int kIslandCells = 256;

// The players start in a clearing.
constexpr float kClearing = 16.0f;

// Above the beach, and below the rock.
constexpr float kLowestTree = 1.0f;
constexpr float kHighestTree = 11.0f;

// From the top 24 bits of a hash, from 0 to 1.
float Unit(uint32_t hash) {
  return static_cast<float>(hash >> 8) / 16777215.0f;
}

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
  Plant();
}

void Terrain::Plant() {
  tree_squares_ = static_cast<int>(2.0f * half_size() / kTreeSpacing);
  tree_in_.assign(tree_squares_ * tree_squares_, -1);
  for (int j = 0; j < tree_squares_; ++j) {
    for (int i = 0; i < tree_squares_; ++i) {
      // Each square rolls its own dice: where in the square its tree
      // stands, whether it grows, which way it faces, and how big it is.
      const auto roll = [this, i, j](uint32_t die) {
        return Unit(Hash(seed_ + 1000 + die, i, j));
      };
      const float x = i * kTreeSpacing - half_size() +
                      kTreeSpacing * (0.2f + 0.6f * roll(0));
      const float z = j * kTreeSpacing - half_size() +
                      kTreeSpacing * (0.2f + 0.6f * roll(1));
      // Woods where the noise is high, and a few lone trees elsewhere.
      const float woods = Fractal(seed_ + 2000, x / 60.0f, z / 60.0f, 3);
      const float chance = std::clamp(0.5f + 3.0f * woods, 0.05f, 0.95f);
      const float height = Height(x, z);
      if (roll(2) >= chance || height < kLowestTree || height > kHighestTree ||
          std::sqrt(x * x + z * z) < kClearing) {
        continue;
      }
      tree_in_[j * tree_squares_ + i] = static_cast<int>(trees_.size());
      trees_.push_back({.x = x,
                        .z = z,
                        .yaw = 2.0f * std::numbers::pi_v<float> * roll(3),
                        .scale = 0.8f + 0.6f * roll(4)});
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
         Height(x, z) >= kSeaLevel - kWading && !InATree(x, z);
}

bool Terrain::InATree(float x, float z) const {
  const int i = static_cast<int>(std::floor((x + half_size()) / kTreeSpacing));
  const int j = static_cast<int>(std::floor((z + half_size()) / kTreeSpacing));
  // A trunk near the side of its square is closer to us than its square is.
  for (int down = std::max(j - 1, 0);
       down <= std::min(j + 1, tree_squares_ - 1); ++down) {
    for (int across = std::max(i - 1, 0);
         across <= std::min(i + 1, tree_squares_ - 1); ++across) {
      const int tree = tree_in_[down * tree_squares_ + across];
      if (tree >= 0 &&
          std::hypot(trees_[tree].x - x, trees_[tree].z - z) < kTrunk) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace nanommo
