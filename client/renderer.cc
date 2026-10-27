#include "client/renderer.h"

#include <raymath.h>
#include <rlgl.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "client/scene.h"

namespace nanommo {

namespace {

constexpr const char* kVertexShader = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matNormal;
out vec3 normal;
out vec4 color;
out float depth;

void main() {
  normal = mat3(matNormal) * vertexNormal;
  color = vertexColor;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
  depth = gl_Position.w;
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
out vec3 normal;
out vec4 color;
out float depth;

void main() {
  mat4 skin = boneMatrices[int(vertexBoneIndices.x)] * vertexBoneWeights.x
            + boneMatrices[int(vertexBoneIndices.y)] * vertexBoneWeights.y
            + boneMatrices[int(vertexBoneIndices.z)] * vertexBoneWeights.z
            + boneMatrices[int(vertexBoneIndices.w)] * vertexBoneWeights.w;
  normal = mat3(matNormal) * mat3(skin) * vertexNormal;
  color = vertexColor;
  gl_Position = mvp * skin * vec4(vertexPosition, 1.0);
  depth = gl_Position.w;
}
)";

// The light is worked out for each pixel, from the normal the vertex shaders
// hand on, so that cel shading's bands follow the surface.
constexpr const char* kFragmentShader = R"(#version 330
in vec3 normal;
in vec4 color;
in float depth;
uniform vec4 colDiffuse;
uniform vec3 sun;
uniform vec3 sunlight;
uniform vec3 ambient;
uniform vec3 fogColor;
uniform float fogNear;
uniform float fogFar;
uniform float emission;
uniform float bands;
out vec4 pixel;

void main() {
  float sunlit = max(dot(normalize(normal), normalize(sun)), 0.0);
  // Cel shading, like raylib's example: the sun's light in a few flat bands,
  // as a cartoon's colours are, where no bands is smooth.
  if (bands > 0.0) {
    sunlit = min(floor(sunlit * bands), bands - 1.0) / (bands - 1.0);
  }
  vec3 light = ambient + sunlight * sunlit + emission;
  vec3 lit = colDiffuse.rgb * color.rgb * light;
  float fog = clamp((depth - fogNear) / (fogFar - fogNear), 0.0, 1.0);
  pixel = vec4(mix(lit, fogColor, fog), colDiffuse.a * color.a);
}
)";

// Cel shading's outlines: each vertex pushed out along its normal, in metres,
// so that the model is drawn a little bigger.
constexpr const char* kOutlineVertexShader = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 matView;
uniform mat4 matProjection;
uniform float thickness;
out float depth;

void main() {
  vec3 normal = normalize(mat3(matNormal) * vertexNormal);
  vec4 at = matModel * vec4(vertexPosition, 1.0)
          + vec4(normal * thickness, 0.0);
  gl_Position = matProjection * matView * at;
  depth = gl_Position.w;
}
)";

// For models with bones, posed like the skinned vertex shader.
constexpr const char* kSkinnedOutlineVertexShader = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexBoneIndices;
in vec4 vertexBoneWeights;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 matView;
uniform mat4 matProjection;
uniform mat4 boneMatrices[128];
uniform float thickness;
out float depth;

void main() {
  mat4 skin = boneMatrices[int(vertexBoneIndices.x)] * vertexBoneWeights.x
            + boneMatrices[int(vertexBoneIndices.y)] * vertexBoneWeights.y
            + boneMatrices[int(vertexBoneIndices.z)] * vertexBoneWeights.z
            + boneMatrices[int(vertexBoneIndices.w)] * vertexBoneWeights.w;
  vec3 normal = normalize(mat3(matNormal) * mat3(skin) * vertexNormal);
  vec4 at = matModel * skin * vec4(vertexPosition, 1.0)
          + vec4(normal * thickness, 0.0);
  gl_Position = matProjection * matView * at;
  depth = gl_Position.w;
}
)";

// Nearly black, into the fog like the rest of the world.
constexpr const char* kOutlineFragmentShader = R"(#version 330
in float depth;
uniform vec3 fogColor;
uniform float fogNear;
uniform float fogFar;
out vec4 pixel;

void main() {
  float fog = clamp((depth - fogNear) / (fogFar - fogNear), 0.0, 1.0);
  pixel = vec4(mix(vec3(0.05), fogColor, fog), 1.0);
}
)";

// In metres, so that an outline is thinner the further away it is.
constexpr float kOutlineThickness = 0.05f;

constexpr std::array<float, 3> kPreviewBackground = {0.12f, 0.14f, 0.19f};

// In metres: a glow's middle is halfway up a player, and it reaches past their
// head and their feet.
constexpr float kMiddle = 0.9f;
constexpr float kGlowRadius = 1.1f;
// A glow gives off half the light of a face turned to a white sun.
constexpr float kGlowEmission = 0.5f;
// The sun's light in three bands: in shadow, half lit and lit.
constexpr float kBands = 3.0f;

// The light what's drawn next gives off of its own, which takes it past white,
// so that it blooms.
void SetEmission(Shader shader, float emission) {
  SetShaderValue(shader, GetShaderLocation(shader, "emission"), &emission,
                 SHADER_UNIFORM_FLOAT);
}

void SetFog(Shader shader, const Atmosphere& atmosphere) {
  // Into the sky's colour, so that far away things fade into the sky.
  SetShaderValue(shader, GetShaderLocation(shader, "fogColor"),
                 atmosphere.sky.data(), SHADER_UNIFORM_VEC3);
  SetShaderValue(shader, GetShaderLocation(shader, "fogNear"),
                 &atmosphere.fog_near, SHADER_UNIFORM_FLOAT);
  SetShaderValue(shader, GetShaderLocation(shader, "fogFar"),
                 &atmosphere.fog_far, SHADER_UNIFORM_FLOAT);
}

void SetAtmosphere(Shader shader, const Atmosphere& atmosphere) {
  SetFog(shader, atmosphere);
  SetShaderValue(shader, GetShaderLocation(shader, "sun"),
                 atmosphere.sun.data(), SHADER_UNIFORM_VEC3);
  SetShaderValue(shader, GetShaderLocation(shader, "sunlight"),
                 atmosphere.sunlight.data(), SHADER_UNIFORM_VEC3);
  SetShaderValue(shader, GetShaderLocation(shader, "ambient"),
                 atmosphere.ambient.data(), SHADER_UNIFORM_VEC3);
}

}  // namespace

Renderer::Renderer(const Content& content)
    : cube_shader_(LoadShaderFromMemory(kVertexShader, kFragmentShader)),
      character_shader_(
          LoadShaderFromMemory(kSkinnedVertexShader, kFragmentShader)),
      outline_{.still = LoadShaderFromMemory(kOutlineVertexShader,
                                             kOutlineFragmentShader),
               .skinned = LoadShaderFromMemory(kSkinnedOutlineVertexShader,
                                               kOutlineFragmentShader)},
      cube_(LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f))),
      sphere_(LoadModelFromMesh(GenMeshSphere(1.0f, 16, 16))),
      characters_(CharactersFolder(), {"x_bot.glb", "y_bot.glb"}, 1.8f,
                  character_shader_) {
  cube_.materials[0].shader = cube_shader_;
  sphere_.materials[0].shader = cube_shader_;
  for (const Shader& shader : {outline_.still, outline_.skinned}) {
    SetShaderValue(shader, GetShaderLocation(shader, "thickness"),
                   &kOutlineThickness, SHADER_UNIFORM_FLOAT);
  }
  // Weapons have no bones, so they're drawn with the cube's shader.
  for (const auto& [id, item] : content.items.by_id) {
    characters_.Hold(id, item.look(), cube_shader_);
  }
  for (const auto& [id, skill] : content.skills.by_id) {
    characters_.Learn(id, skill.look());
    if (skill.look().has_glow()) {
      glows_[id] = GetColor(skill.look().glow());
    }
  }
  // Two kinds with one model load it twice, which is fine for a few kinds.
  for (const auto& [id, kind] : content.monsters.kinds) {
    monsters_[id] = std::make_unique<Characters>(
        MonstersFolder(), std::vector<std::string>{kind.look().model()},
        kind.look().height(), character_shader_);
  }

  tree_ = LoadFaces(TreesFolder() + "/tree.glb");
}

// UnloadModel leaves the model's shader alone, as models can share one.
Renderer::~Renderer() {
  UnloadTerrain();
  UnloadModel(cube_);
  UnloadModel(sphere_);
  UnloadShader(character_shader_);
  UnloadShader(outline_.still);
  UnloadShader(outline_.skinned);
  UnloadShader(cube_shader_);
}

void Renderer::set_terrain(const Terrain& terrain, const Look& look) {
  UnloadTerrain();
  atmosphere_ = look.atmosphere;
  // The ground and the trees have no bones either.
  chunks_ = LoadChunks(terrain, look, tree_, cube_shader_);
  sea_ = std::make_unique<Sea>(look);
}

void Renderer::set_cel_shading(bool on) {
  if (on == cel_shading_) {
    return;
  }
  cel_shading_ = on;
  // A shader keeps a uniform's value until it's set again. With no bands,
  // which a uniform starts at, the light is smooth.
  const float bands = on ? kBands : 0.0f;
  for (const Shader& shader : {cube_shader_, character_shader_}) {
    SetShaderValue(shader, GetShaderLocation(shader, "bands"), &bands,
                   SHADER_UNIFORM_FLOAT);
  }
}

void Renderer::UnloadTerrain() {
  UnloadChunks(chunks_);
  sea_.reset();
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
  SetFog(outline_.still, atmosphere_);
  SetFog(outline_.skinned, atmosphere_);
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
  }
  // Last, as we see the ground under it through the water.
  if (sea_) {
    sea_->Draw(camera_, atmosphere_, cel_shading_ ? kBands : 0.0f,
               static_cast<float>(GetTime()));
    ++work_.draws;
    work_.triangles += sea_->triangles();
  }
  // The glows are see-through too, around the players using a skill with one.
  // Each grows over the first quarter of a second. A glow is light, so it adds
  // to what's behind it, where blending would cover it.
  SetEmission(cube_shader_, kGlowEmission);
  BeginBlendMode(BLEND_ADDITIVE);
  for (const Figure& figure : figures_) {
    const auto glow = glows_.find(figure.to_skill);
    if (figure.to == protocol::MOTION_SKILL && glow != glows_.end()) {
      const Vector3& at = figure.position;
      const float radius = kGlowRadius * figure.blend;
      DrawModelEx(sphere_, {at.x, at.y + kMiddle, at.z}, {0.0f, 1.0f, 0.0f},
                  0.0f, {radius, radius, radius}, glow->second);
    }
  }
  EndBlendMode();
  SetEmission(cube_shader_, 0.0f);
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
  SetFog(outline_.still, preview);
  SetFog(outline_.skinned, preview);
  BeginMode3D(camera);
  DrawFigure(figure);
  EndMode3D();
}

void Renderer::DrawFigure(const Figure& figure) const {
  if (figure.npc) {
    // The ball's middle is its origin, so it's raised to rest on the ground.
    const Vector3& at = figure.position;
    const float radius = figure.radius;
    if (cel_shading_) {
      DrawOutline(sphere_,
                  MatrixMultiply(MatrixScale(radius, radius, radius),
                                 MatrixTranslate(at.x, at.y + radius, at.z)),
                  outline_.still);
    }
    DrawModelEx(sphere_, {at.x, at.y + radius, at.z}, {0.0f, 1.0f, 0.0f}, 0.0f,
                {radius, radius, radius}, figure.color);
    return;
  }
  const Characters* looks = &characters_;
  if (figure.monster) {
    const auto kind = monsters_.find(figure.kind);
    looks = kind != monsters_.end() ? kind->second.get() : nullptr;
  }
  const Color color = figure.monster ? figure.color : PlayerColor(figure.id);
  if (looks != nullptr && looks->size() > 0) {
    looks->Draw(figure, color, cel_shading_ ? &outline_ : nullptr);
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
