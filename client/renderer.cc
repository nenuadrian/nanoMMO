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
uniform vec3 sun;
uniform vec3 sunlight;
uniform vec3 ambient;
out vec3 light;
out vec4 color;
out float fog;

void main() {
  vec3 normal = normalize(mat3(matNormal) * vertexNormal);
  light = ambient + sunlight * max(dot(normal, normalize(sun)), 0.0);
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
uniform vec3 sun;
uniform vec3 sunlight;
uniform vec3 ambient;
out vec3 light;
out vec4 color;
out float fog;

void main() {
  mat4 skin = boneMatrices[int(vertexBoneIndices.x)] * vertexBoneWeights.x
            + boneMatrices[int(vertexBoneIndices.y)] * vertexBoneWeights.y
            + boneMatrices[int(vertexBoneIndices.z)] * vertexBoneWeights.z
            + boneMatrices[int(vertexBoneIndices.w)] * vertexBoneWeights.w;
  vec3 normal = normalize(mat3(matNormal) * mat3(skin) * vertexNormal);
  light = ambient + sunlight * max(dot(normal, normalize(sun)), 0.0);
  color = vertexColor;
  gl_Position = mvp * skin * vec4(vertexPosition, 1.0);
  fog = clamp((gl_Position.w - fogNear) / (fogFar - fogNear), 0.0, 1.0);
}
)";

constexpr const char* kFragmentShader = R"(#version 330
in vec3 light;
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

void SetAtmosphere(Shader shader, const Atmosphere& atmosphere) {
  // Into the sky's colour, so that far away things fade into the sky.
  SetShaderValue(shader, GetShaderLocation(shader, "fogColor"),
                 atmosphere.sky.data(), SHADER_UNIFORM_VEC3);
  SetShaderValue(shader, GetShaderLocation(shader, "fogNear"),
                 &atmosphere.fog_near, SHADER_UNIFORM_FLOAT);
  SetShaderValue(shader, GetShaderLocation(shader, "fogFar"),
                 &atmosphere.fog_far, SHADER_UNIFORM_FLOAT);
  SetShaderValue(shader, GetShaderLocation(shader, "sun"),
                 atmosphere.sun.data(), SHADER_UNIFORM_VEC3);
  SetShaderValue(shader, GetShaderLocation(shader, "sunlight"),
                 atmosphere.sunlight.data(), SHADER_UNIFORM_VEC3);
  SetShaderValue(shader, GetShaderLocation(shader, "ambient"),
                 atmosphere.ambient.data(), SHADER_UNIFORM_VEC3);
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

void Renderer::set_terrain(const Terrain& terrain, const Look& look) {
  UnloadTerrain();
  atmosphere_ = look.atmosphere;
  // The ground, the trees and the sea have no bones either.
  chunks_ = LoadChunks(terrain, look, tree_, cube_shader_);
  sea_ = LoadModelFromMesh(SeaMesh(terrain));
  sea_->materials[0].shader = cube_shader_;
  sea_->materials[0].maps[MATERIAL_MAP_DIFFUSE].color = look.sea;
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
  SetAtmosphere(cube_shader_, atmosphere_);
  SetAtmosphere(character_shader_, atmosphere_);
  BeginMode3D(camera_);
  // As BeginMode3D set them up, so we cull with what the GPU will use.
  const Matrix view_projection =
      MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
  for (const Chunk& chunk : chunks_) {
    if (InSight(chunk.box, camera_.position, view_projection,
                atmosphere_.fog_far)) {
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
  // Lit like the Meadows, wherever we are.
  Atmosphere preview = kMeadows.atmosphere;
  preview.sky = kPreviewBackground;
  SetAtmosphere(cube_shader_, preview);
  SetAtmosphere(character_shader_, preview);
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
  const std::array<float, 3>& sky = atmosphere_.sky;
  ClearBackground(ColorFromNormalized({sky[0], sky[1], sky[2], 1.0f}));
}

}  // namespace nanommo
