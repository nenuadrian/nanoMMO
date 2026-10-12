#include "client/ground.h"

#include <algorithm>
#include <cstdint>

#include "terrain/noise.h"

namespace nanommo {

namespace {

constexpr Color kSand{214, 196, 140, 255};
constexpr Color kGrass{98, 160, 72, 255};
constexpr Color kRock{128, 120, 112, 255};
constexpr Color kSnow{238, 240, 245, 255};

constexpr float kBeach = 0.6f;
constexpr float kTreeLine = 12.0f;
constexpr float kSnowLine = 19.0f;

// The y of a face's normal: below this, it's too steep for grass.
constexpr float kSteep = 0.8f;

Color GroundColor(float height, float up) {
  if (height < kBeach) {
    return kSand;
  }
  if (height > kSnowLine) {
    return kSnow;
  }
  if (height > kTreeLine || up < kSteep) {
    return kRock;
  }
  return kGrass;
}

// Up to four percent lighter or darker, so that faces of one colour still
// stand apart.
Color Vary(Color color, uint32_t hash) {
  const float shade = 0.96f + 0.08f * static_cast<float>(hash & 0xff) / 255.0f;
  const auto scaled = [shade](unsigned char value) {
    return static_cast<unsigned char>(std::min(value * shade, 255.0f));
  };
  return {scaled(color.r), scaled(color.g), scaled(color.b), color.a};
}

void AddFace(Triangles& triangles, Vector3 a, Vector3 b, Vector3 c,
             uint32_t hash) {
  const float height = (a.y + b.y + c.y) / 3.0f;
  const Color color = GroundColor(height, FaceNormal(a, b, c).y);
  triangles.Add(a, b, c, Vary(color, hash));
}

}  // namespace

void AddGround(Triangles& triangles, const Terrain& terrain, int left, int top,
               int cells) {
  const float half_size = terrain.half_size();
  const auto corner = [&](int i, int j) {
    return Vector3{i * Terrain::kCell - half_size, terrain.Corner(i, j),
                   j * Terrain::kCell - half_size};
  };
  for (int j = top; j < std::min(top + cells, terrain.cells()); ++j) {
    for (int i = left; i < std::min(left + cells, terrain.cells()); ++i) {
      const Vector3 top_left = corner(i, j);
      const Vector3 top_right = corner(i + 1, j);
      const Vector3 bottom_left = corner(i, j + 1);
      const Vector3 bottom_right = corner(i + 1, j + 1);
      AddFace(triangles, top_left, bottom_right, top_right,
              Hash(terrain.seed(), 2 * i, j));
      AddFace(triangles, top_left, bottom_left, bottom_right,
              Hash(terrain.seed(), 2 * i + 1, j));
    }
  }
}

Mesh SeaMesh(const Terrain& terrain) {
  const float size = 8.0f * terrain.half_size();
  return GenMeshPlane(size, size, 1, 1);
}

}  // namespace nanommo
