#include "client/renderer.h"

#include "client/camera.h"

namespace nanommo {

namespace {

constexpr const char* kVertexShader = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matNormal;
out float light;

void main() {
  vec3 normal = normalize(mat3(matNormal) * vertexNormal);
  vec3 sun = normalize(vec3(0.3, 1.0, 0.5));
  light = 0.55 + 0.45 * max(dot(normal, sun), 0.0);
  gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

constexpr const char* kFragmentShader = R"(#version 330
in float light;
uniform vec4 colDiffuse;
out vec4 pixel;

void main() {
  pixel = vec4(colDiffuse.rgb * light, colDiffuse.a);
}
)";

}  // namespace

Renderer::Renderer()
    : shader_(LoadShaderFromMemory(kVertexShader, kFragmentShader)),
      cube_(LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f))) {
  cube_.materials[0].shader = shader_;
}

// UnloadModel leaves the model's shader alone, as models can share one.
Renderer::~Renderer() {
  UnloadModel(cube_);
  UnloadShader(shader_);
}

void Renderer::Draw() const {
  ClearBackground(ColorFromNormalized(
      {background_[0], background_[1], background_[2], 1.0f}));

  BeginMode3D(GameCamera());
  for (const Cube& cube : cubes_) {
    DrawModelEx(cube_, cube.position, {0.0f, 1.0f, 0.0f}, 0.0f, cube.size,
                cube.color);
  }
  EndMode3D();
}

}  // namespace nanommo
