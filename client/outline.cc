#include "client/outline.h"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <vector>

namespace nanommo {

void DrawOutline(const Model& model, const Matrix& transform, Shader shader) {
  // A copy of the model, with the outline's shader in its materials, so that
  // raylib draws it posed, with its bones, as it does the model.
  std::vector<Material> materials(model.materials,
                                  model.materials + model.materialCount);
  for (Material& material : materials) {
    material.shader = shader;
  }
  Model hull = model;
  hull.materials = materials.data();
  hull.transform = MatrixMultiply(model.transform, transform);
  rlSetCullFace(RL_CULL_FACE_FRONT);
  DrawModel(hull, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
  rlSetCullFace(RL_CULL_FACE_BACK);
}

}  // namespace nanommo
