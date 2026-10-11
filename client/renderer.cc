#include "client/renderer.h"

#include "client/camera.h"
#include "client/scene.h"

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

constexpr const char* kSkinnedVertexShader = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexBoneIndices;
in vec4 vertexBoneWeights;
uniform mat4 mvp;
uniform mat4 matNormal;
uniform mat4 boneMatrices[128];
out float light;

void main() {
  mat4 skin = boneMatrices[int(vertexBoneIndices.x)] * vertexBoneWeights.x
            + boneMatrices[int(vertexBoneIndices.y)] * vertexBoneWeights.y
            + boneMatrices[int(vertexBoneIndices.z)] * vertexBoneWeights.z
            + boneMatrices[int(vertexBoneIndices.w)] * vertexBoneWeights.w;
  vec3 normal = normalize(mat3(matNormal) * mat3(skin) * vertexNormal);
  vec3 sun = normalize(vec3(0.3, 1.0, 0.5));
  light = 0.55 + 0.45 * max(dot(normal, sun), 0.0);
  gl_Position = mvp * skin * vec4(vertexPosition, 1.0);
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
    : cube_shader_(LoadShaderFromMemory(kVertexShader, kFragmentShader)),
      character_shader_(
          LoadShaderFromMemory(kSkinnedVertexShader, kFragmentShader)),
      cube_(LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f))),
      characters_(CharactersFolder(), {"x_bot.glb", "y_bot.glb"}, 1.8f,
                  character_shader_),
      monsters_(MonstersFolder(), {"skeleton.glb"}, 1.6f, character_shader_) {
  cube_.materials[0].shader = cube_shader_;
}

// UnloadModel leaves the model's shader alone, as models can share one.
Renderer::~Renderer() {
  UnloadModel(cube_);
  UnloadShader(character_shader_);
  UnloadShader(cube_shader_);
}

void Renderer::Draw() const {
  Clear();
  BeginMode3D(GameCamera());
  for (const Cube& cube : cubes_) {
    DrawModelEx(cube_, cube.position, {0.0f, 1.0f, 0.0f}, 0.0f, cube.size,
                cube.color);
  }
  for (const Figure& figure : figures_) {
    const Characters& looks = figure.monster ? monsters_ : characters_;
    const Color color = figure.monster ? WHITE : PlayerColor(figure.id);
    if (looks.size() > 0) {
      looks.Draw(figure, color);
    } else {
      const Vector3& at = figure.position;
      DrawModelEx(cube_, {at.x, 0.5f, at.z}, {0.0f, 1.0f, 0.0f}, 0.0f,
                  {1.0f, 1.0f, 1.0f}, color);
    }
    if (figure.you) {
      const Vector3& at = figure.position;
      DrawModelEx(cube_, {at.x, kMarkerHeight, at.z}, {0.0f, 1.0f, 0.0f}, 0.0f,
                  {0.25f, 0.25f, 0.25f}, WHITE);
    }
  }
  EndMode3D();
}

void Renderer::Clear() const {
  ClearBackground(ColorFromNormalized(
      {background_[0], background_[1], background_[2], 1.0f}));
}

}  // namespace nanommo
