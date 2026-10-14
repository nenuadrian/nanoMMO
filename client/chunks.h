#ifndef NANOMMO_CLIENT_CHUNKS_H_
#define NANOMMO_CLIENT_CHUNKS_H_

#include <raylib.h>

#include <vector>

#include "client/atmosphere.h"
#include "client/trees.h"
#include "terrain/terrain.h"

namespace nanommo {

/// In cells along each side: 32 metres.
constexpr int kChunkCells = 16;

/// A square of the terrain, its ground and its trees in one mesh, so that
/// one draw draws all of it.
struct Chunk {
  Model model;
  BoundingBox box;
};

std::vector<Chunk> LoadChunks(const Terrain& terrain, const Look& look,
                              const std::vector<Face>& tree, Shader shader);

void UnloadChunks(std::vector<Chunk>& chunks);

/// Whether any of the box is closer to the eye than the farthest we see, and
/// inside the view: past none of its six sides.
bool InSight(const BoundingBox& box, Vector3 eye, const Matrix& view_projection,
             float farthest);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_CHUNKS_H_
