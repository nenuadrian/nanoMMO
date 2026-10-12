#include "client/trees.h"

#include <raymath.h>

#include <algorithm>
#include <string>
#include <vector>

namespace nanommo {

namespace {

// So that the trunk doesn't float over a slope.
constexpr float kSunk = 0.15f;

Color ColorAt(const Image& texture, float u, float v) {
  const int x =
      std::clamp(static_cast<int>(u * texture.width), 0, texture.width - 1);
  const int y =
      std::clamp(static_cast<int>(v * texture.height), 0, texture.height - 1);
  return GetImageColor(texture, x, y);
}

}  // namespace

std::string TreesFolder() {
  return std::string(GetApplicationDirectory()) + "trees";
}

std::vector<Face> LoadFaces(const std::string& path) {
  if (!FileExists(path.c_str())) {
    return {};
  }
  Model model = LoadModel(path.c_str());
  const BoundingBox box = GetModelBoundingBox(model);
  const float scale = 1.0f / (box.max.y - box.min.y);
  std::vector<Face> faces;
  for (int m = 0; m < model.meshCount; ++m) {
    const Mesh& mesh = model.meshes[m];
    const Material& material = model.materials[model.meshMaterial[m]];
    // raylib has uploaded the texture already, so we read it back.
    Image texture =
        LoadImageFromTexture(material.maps[MATERIAL_MAP_DIFFUSE].texture);
    const auto corner = [&](int i) {
      return Vector3{mesh.vertices[3 * i] * scale,
                     (mesh.vertices[3 * i + 1] - box.min.y) * scale,
                     mesh.vertices[3 * i + 2] * scale};
    };
    for (int t = 0; t < mesh.triangleCount; ++t) {
      int corners[3];
      float u = 0.0f;
      float v = 0.0f;
      for (int k = 0; k < 3; ++k) {
        corners[k] = mesh.indices ? mesh.indices[3 * t + k] : 3 * t + k;
        if (mesh.texcoords) {
          u += mesh.texcoords[2 * corners[k]] / 3.0f;
          v += mesh.texcoords[2 * corners[k] + 1] / 3.0f;
        }
      }
      faces.push_back({.a = corner(corners[0]),
                       .b = corner(corners[1]),
                       .c = corner(corners[2]),
                       .color = ColorAt(texture, u, v)});
    }
    UnloadImage(texture);
  }
  UnloadModel(model);
  return faces;
}

Matrix TreeTransform(const Tree& tree, const Terrain& terrain) {
  const float size = kTreeHeight * tree.scale;
  return MatrixMultiply(
      MatrixMultiply(MatrixScale(size, size, size), MatrixRotateY(tree.yaw)),
      MatrixTranslate(tree.x, terrain.Height(tree.x, tree.z) - kSunk, tree.z));
}

void AddFaces(Triangles& triangles, const std::vector<Face>& faces,
              const Matrix& transform) {
  for (const Face& face : faces) {
    triangles.Add(Vector3Transform(face.a, transform),
                  Vector3Transform(face.b, transform),
                  Vector3Transform(face.c, transform), face.color);
  }
}

}  // namespace nanommo
