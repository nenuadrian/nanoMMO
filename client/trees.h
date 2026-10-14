#ifndef NANOMMO_CLIENT_TREES_H_
#define NANOMMO_CLIENT_TREES_H_

#include <raylib.h>

#include <string>
#include <vector>

#include "client/triangles.h"
#include "terrain/terrain.h"

namespace nanommo {

/// In metres, before each tree's own scale.
constexpr float kTreeHeight = 5.0f;

std::string TreesFolder();

struct Face {
  Vector3 a;
  Vector3 b;
  Vector3 c;
  Color color;
};

/// The model's triangles, standing on y = 0 and a metre tall, each in the
/// colour of its texture under the middle of the face. None if the file
/// isn't there.
std::vector<Face> LoadFaces(const std::string& path);

/// Sizes the model, turns it, and stands it on the ground.
Matrix TreeTransform(const Tree& tree, const Terrain& terrain);

/// Adds the faces, moved by the transform, with their colours multiplied by
/// the tint.
void AddFaces(Triangles& triangles, const std::vector<Face>& faces,
              const Matrix& transform, Color tint);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_TREES_H_
