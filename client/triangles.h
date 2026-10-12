#ifndef NANOMMO_CLIENT_TRIANGLES_H_
#define NANOMMO_CLIENT_TRIANGLES_H_

#include <raylib.h>

#include <vector>

namespace nanommo {

/// Pointing out of the side where the corners go anticlockwise, which is the
/// side OpenGL draws.
Vector3 FaceNormal(Vector3 a, Vector3 b, Vector3 c);

/// Triangles that share no corners, so that each has its face's normal, and
/// the light shades each face flat, like a low-poly model.
class Triangles {
 public:
  void Add(Vector3 a, Vector3 b, Vector3 c, Color color);

  int size() const { return static_cast<int>(colors_.size() / 12); }

  /// Hands them to the GPU, as a mesh for LoadModelFromMesh.
  Mesh Upload() const;

 private:
  std::vector<float> positions_;
  std::vector<float> normals_;
  std::vector<unsigned char> colors_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_TRIANGLES_H_
