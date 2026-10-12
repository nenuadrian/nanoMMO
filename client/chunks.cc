#include "client/chunks.h"

#include <raymath.h>

#include <algorithm>
#include <array>
#include <vector>

#include "client/ground.h"
#include "client/triangles.h"

namespace nanommo {

std::vector<Chunk> LoadChunks(const Terrain& terrain,
                              const std::vector<Face>& tree, Shader shader) {
  const int across = (terrain.cells() + kChunkCells - 1) / kChunkCells;
  std::vector<Triangles> squares(across * across);
  for (int j = 0; j < across; ++j) {
    for (int i = 0; i < across; ++i) {
      AddGround(squares[j * across + i], terrain, i * kChunkCells,
                j * kChunkCells, kChunkCells);
    }
  }
  // Each tree goes in with the ground under its trunk.
  const float size = kChunkCells * Terrain::kCell;
  for (const Tree& planted : terrain.trees()) {
    const int i =
        std::clamp(static_cast<int>((planted.x + terrain.half_size()) / size),
                   0, across - 1);
    const int j =
        std::clamp(static_cast<int>((planted.z + terrain.half_size()) / size),
                   0, across - 1);
    AddFaces(squares[j * across + i], tree, TreeTransform(planted, terrain));
  }

  std::vector<Chunk> chunks;
  for (const Triangles& square : squares) {
    const Mesh mesh = square.Upload();
    Chunk chunk{.model = LoadModelFromMesh(mesh),
                .box = GetMeshBoundingBox(mesh)};
    chunk.model.materials[0].shader = shader;
    chunks.push_back(chunk);
  }
  return chunks;
}

void UnloadChunks(std::vector<Chunk>& chunks) {
  for (Chunk& chunk : chunks) {
    UnloadModel(chunk.model);
  }
  chunks.clear();
}

bool InSight(const BoundingBox& box, Vector3 eye, const Matrix& view_projection,
             float farthest) {
  const Vector3 nearest = Vector3Clamp(eye, box.min, box.max);
  if (Vector3Distance(eye, nearest) > farthest) {
    return false;
  }
  // On the screen, a point's x, y and z are between -w and w. The box is out
  // of sight if all eight of its corners are past the same one of those.
  std::array<int, 6> past{};
  for (int corner = 0; corner < 8; ++corner) {
    const Vector3 point{corner & 1 ? box.max.x : box.min.x,
                        corner & 2 ? box.max.y : box.min.y,
                        corner & 4 ? box.max.z : box.min.z};
    const Matrix& m = view_projection;
    const std::array<float, 3> clip = {
        m.m0 * point.x + m.m4 * point.y + m.m8 * point.z + m.m12,
        m.m1 * point.x + m.m5 * point.y + m.m9 * point.z + m.m13,
        m.m2 * point.x + m.m6 * point.y + m.m10 * point.z + m.m14};
    const float w = m.m3 * point.x + m.m7 * point.y + m.m11 * point.z + m.m15;
    for (int axis = 0; axis < 3; ++axis) {
      past[2 * axis] += clip[axis] < -w;
      past[2 * axis + 1] += clip[axis] > w;
    }
  }
  return std::ranges::none_of(past, [](int corners) { return corners == 8; });
}

}  // namespace nanommo
