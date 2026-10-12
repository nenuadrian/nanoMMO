#include "client/triangles.h"

#include <raymath.h>

#include <algorithm>
#include <vector>

namespace nanommo {

namespace {

template <typename T>
T* Copy(const std::vector<T>& values) {
  // UnloadMesh frees what's here, so raylib allocates it.
  T* copy = static_cast<T*>(MemAlloc(values.size() * sizeof(T)));
  std::ranges::copy(values, copy);
  return copy;
}

}  // namespace

Vector3 FaceNormal(Vector3 a, Vector3 b, Vector3 c) {
  return Vector3Normalize(
      Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
}

void Triangles::Add(Vector3 a, Vector3 b, Vector3 c, Color color) {
  const Vector3 normal = FaceNormal(a, b, c);
  for (const Vector3& corner : {a, b, c}) {
    positions_.insert(positions_.end(), {corner.x, corner.y, corner.z});
    normals_.insert(normals_.end(), {normal.x, normal.y, normal.z});
    colors_.insert(colors_.end(), {color.r, color.g, color.b, color.a});
  }
}

Mesh Triangles::Upload() const {
  Mesh mesh{};
  mesh.triangleCount = size();
  mesh.vertexCount = 3 * size();
  mesh.vertices = Copy(positions_);
  mesh.normals = Copy(normals_);
  mesh.colors = Copy(colors_);
  UploadMesh(&mesh, false);
  return mesh;
}

}  // namespace nanommo
