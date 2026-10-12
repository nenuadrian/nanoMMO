#include "client/renderer.h"

#include <raymath.h>
#include <rlgl.h>

#include <array>
#include <vector>

#include "client/ground.h"
#include "client/scene.h"

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

constexpr std::array<float, 3> kPreviewBackground = {0.12f, 0.14f, 0.19f};

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

  tree_ = LoadFaces(TreesFolder() + "/tree.glb");
}

// UnloadModel leaves the model's shader alone, as models can share one.
Renderer::~Renderer() {
  UnloadTerrain();
  UnloadModel(cube_);
  UnloadShader(character_shader_);
  UnloadShader(cube_shader_);
}

void Renderer::set_terrain(const Terrain& terrain) {
  UnloadTerrain();
  // The ground, the trees and the sea have no bones either.
  chunks_ = LoadChunks(terrain, tree_, cube_shader_);
  sea_ = LoadModelFromMesh(SeaMesh(terrain));
  sea_->materials[0].shader = cube_shader_;
  sea_->materials[0].maps[MATERIAL_MAP_DIFFUSE].color = kSea;
}

void Renderer::UnloadTerrain() {
  UnloadChunks(chunks_);
  if (sea_) {
    UnloadModel(*sea_);
    sea_.reset();
  }
}

void Renderer::DrawCounted(const Mesh& mesh, const Material& material,
                           const Matrix& transform) const {
  DrawMesh(mesh, material, transform);
  ++work_.draws;
  work_.triangles += mesh.triangleCount;
}

void Renderer::Draw() const {
  work_ = {};
  Clear();
  SetFog(cube_shader_, background_);
  SetFog(character_shader_, background_);
  BeginMode3D(camera_);
  // As BeginMode3D set them up, so we cull with what the GPU will use.
  const Matrix view_projection =
      MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
  for (const Chunk& chunk : chunks_) {
    if (InSight(chunk.box, camera_.position, view_projection, kFogFar)) {
      DrawCounted(chunk.model.meshes[0], chunk.model.materials[0],
                  MatrixIdentity());
    }
  }
  for (const Cube& cube : cubes_) {
    DrawModelEx(cube_, cube.position, {0.0f, 1.0f, 0.0f}, 0.0f, cube.size,
                cube.color);
  }
  for (const Figure& figure : figures_) {
    DrawFigure(figure);
    if (figure.you) {
      const Vector3& at = figure.position;
      DrawModelEx(cube_, {at.x, at.y + kMarkerHeight, at.z}, {0.0f, 1.0f, 0.0f},
                  0.0f, {0.25f, 0.25f, 0.25f}, WHITE);
    }
  }
  // Last, as we see the ground under it through the water.
  if (sea_) {
    DrawCounted(sea_->meshes[0], sea_->materials[0],
                MatrixTranslate(0.0f, Terrain::kSeaLevel, 0.0f));
  }
  EndMode3D();
}

void Renderer::DrawPreview(const Figure& figure, const Camera3D& camera) const {
  ClearBackground(
      ColorFromNormalized({kPreviewBackground[0], kPreviewBackground[1],
                           kPreviewBackground[2], 1.0f}));
  SetFog(cube_shader_, kPreviewBackground);
  SetFog(character_shader_, kPreviewBackground);
  BeginMode3D(camera);
  DrawFigure(figure);
  EndMode3D();
}

void Renderer::DrawFigure(const Figure& figure) const {
  const Characters& looks = figure.monster ? monsters_ : characters_;
  const Color color = figure.monster ? WHITE : PlayerColor(figure.id);
  if (looks.size() > 0) {
    looks.Draw(figure, color);
    return;
  }
  const Vector3& at = figure.position;
  DrawModelEx(cube_, {at.x, at.y + 0.5f, at.z}, {0.0f, 1.0f, 0.0f}, 0.0f,
              {1.0f, 1.0f, 1.0f}, color);
}

void Renderer::Clear() const {
  ClearBackground(ColorFromNormalized(
      {background_[0], background_[1], background_[2], 1.0f}));
}

}  // namespace nanommo
