#include "client/renderer.h"

#include <raymath.h>

#include <array>
#include <vector>

#include "client/ground.h"
#include "client/scene.h"
#include "client/triangles.h"

namespace nanommo {

namespace {

constexpr const char* kVertexShader = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matNormal;
uniform float fogNear;
uniform float fogFar;
out float light;
out vec4 color;
out float fog;

void main() {
  vec3 normal = normalize(mat3(matNormal) * vertexNormal);
  vec3 sun = normalize(vec3(0.3, 1.0, 0.5));
  light = 0.55 + 0.45 * max(dot(normal, sun), 0.0);
  color = vertexColor;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
  fog = clamp((gl_Position.w - fogNear) / (fogFar - fogNear), 0.0, 1.0);
}
)";

constexpr const char* kSkinnedVertexShader = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexBoneIndices;
in vec4 vertexBoneWeights;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matNormal;
uniform mat4 boneMatrices[128];
uniform float fogNear;
uniform float fogFar;
out float light;
out vec4 color;
out float fog;

void main() {
  mat4 skin = boneMatrices[int(vertexBoneIndices.x)] * vertexBoneWeights.x
            + boneMatrices[int(vertexBoneIndices.y)] * vertexBoneWeights.y
            + boneMatrices[int(vertexBoneIndices.z)] * vertexBoneWeights.z
            + boneMatrices[int(vertexBoneIndices.w)] * vertexBoneWeights.w;
  vec3 normal = normalize(mat3(matNormal) * mat3(skin) * vertexNormal);
  vec3 sun = normalize(vec3(0.3, 1.0, 0.5));
  light = 0.55 + 0.45 * max(dot(normal, sun), 0.0);
  color = vertexColor;
  gl_Position = mvp * skin * vec4(vertexPosition, 1.0);
  fog = clamp((gl_Position.w - fogNear) / (fogFar - fogNear), 0.0, 1.0);
}
)";

constexpr const char* kFragmentShader = R"(#version 330
in float light;
in vec4 color;
in float fog;
uniform vec4 colDiffuse;
uniform vec3 fogColor;
out vec4 pixel;

void main() {
  vec3 lit = colDiffuse.rgb * color.rgb * light;
  pixel = vec4(mix(lit, fogColor, fog), colDiffuse.a * color.a);
}
)";

// Into the background's colour, so that far away things fade into the sky.
void SetFog(Shader shader, const std::array<float, 3>& color) {
  SetShaderValue(shader, GetShaderLocation(shader, "fogColor"), color.data(),
                 SHADER_UNIFORM_VEC3);
  SetShaderValue(shader, GetShaderLocation(shader, "fogNear"),
                 &Renderer::kFogNear, SHADER_UNIFORM_FLOAT);
  SetShaderValue(shader, GetShaderLocation(shader, "fogFar"),
                 &Renderer::kFogFar, SHADER_UNIFORM_FLOAT);
}

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
  // The sword has no bones, so it's drawn with the cube's shader.
  characters_.Hold(WeaponsFolder() + "/sword.glb", 1.0f, cube_shader_);

  const std::vector<Face> faces = LoadFaces(TreesFolder() + "/tree.glb");
  if (!faces.empty()) {
    Triangles triangles;
    AddFaces(triangles, faces, MatrixIdentity());
    tree_ = LoadModelFromMesh(triangles.Upload());
    tree_->materials[0].shader = cube_shader_;
  }
}

// UnloadModel leaves the model's shader alone, as models can share one.
Renderer::~Renderer() {
  UnloadTerrain();
  if (tree_) {
    UnloadModel(*tree_);
  }
  UnloadModel(cube_);
  UnloadShader(character_shader_);
  UnloadShader(cube_shader_);
}

void Renderer::set_terrain(const Terrain& terrain) {
  UnloadTerrain();
  ground_ = LoadModelFromMesh(GroundMesh(terrain));
  sea_ = LoadModelFromMesh(SeaMesh(terrain));
  // The ground and the sea have no bones either.
  ground_->materials[0].shader = cube_shader_;
  sea_->materials[0].shader = cube_shader_;
  trees_.clear();
  for (const Tree& tree : terrain.trees()) {
    trees_.push_back(TreeTransform(tree, terrain));
  }
}

void Renderer::UnloadTerrain() {
  if (ground_) {
    UnloadModel(*ground_);
    UnloadModel(*sea_);
    ground_.reset();
    sea_.reset();
  }
}

void Renderer::Draw() const {
  Clear();
  SetFog(cube_shader_, background_);
  SetFog(character_shader_, background_);
  BeginMode3D(camera_);
  if (ground_) {
    DrawModel(*ground_, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
  }
  if (tree_) {
    for (const Matrix& tree : trees_) {
      DrawMesh(tree_->meshes[0], tree_->materials[0], tree);
    }
  }
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
      DrawModelEx(cube_, {at.x, at.y + 0.5f, at.z}, {0.0f, 1.0f, 0.0f}, 0.0f,
                  {1.0f, 1.0f, 1.0f}, color);
    }
    if (figure.you) {
      const Vector3& at = figure.position;
      DrawModelEx(cube_, {at.x, at.y + kMarkerHeight, at.z}, {0.0f, 1.0f, 0.0f},
                  0.0f, {0.25f, 0.25f, 0.25f}, WHITE);
    }
  }
  // Last, as we see the ground under it through the water.
  if (sea_) {
    DrawModel(*sea_, {0.0f, Terrain::kSeaLevel, 0.0f}, 1.0f, kSea);
  }
  EndMode3D();
}

void Renderer::Clear() const {
  ClearBackground(ColorFromNormalized(
      {background_[0], background_[1], background_[2], 1.0f}));
}

}  // namespace nanommo
