#include "client/renderer.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "client/glsl.h"
#include "client/picking.h"
#include "client/scene.h"
#include "client/sky.h"
#include "server/duels.h"

namespace nanommo {

namespace {

constexpr const char* kVertexShader = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 normal;
out vec4 color;
out float depth;
out vec3 world;

void main() {
  normal = mat3(matNormal) * vertexNormal;
  color = vertexColor;
  world = (matModel * vec4(vertexPosition, 1.0)).xyz;
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
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 boneMatrices[128];
out vec3 normal;
out vec4 color;
out float depth;
out vec3 world;

void main() {
  mat4 skin = boneMatrices[int(vertexBoneIndices.x)] * vertexBoneWeights.x
            + boneMatrices[int(vertexBoneIndices.y)] * vertexBoneWeights.y
            + boneMatrices[int(vertexBoneIndices.z)] * vertexBoneWeights.z
            + boneMatrices[int(vertexBoneIndices.w)] * vertexBoneWeights.w;
  normal = mat3(matNormal) * mat3(skin) * vertexNormal;
  color = vertexColor;
  world = (matModel * skin * vec4(vertexPosition, 1.0)).xyz;
  gl_Position = mvp * skin * vec4(vertexPosition, 1.0);
  depth = gl_Position.w;
}
)";

// The light is worked out for each pixel, from the normal the vertex shaders
// hand on, so that cel shading's bands follow the surface. Where a cloud is
// between the pixel and the sun, less of the sun's light gets through.
constexpr const char* kFragmentShader = R"(#version 330
in vec3 normal;
in vec4 color;
in float depth;
in vec3 world;
uniform vec4 colDiffuse;
// The middle of each ball the clouds are made of, x and z, its height, and
// how wide it is, or 0 for none.
uniform vec4 clouds[64];
uniform float cloudShade;
uniform vec3 sun;
uniform vec3 sunlight;
uniform vec3 ambient;
uniform vec3 fogColor;
uniform float fogNear;
uniform float fogFar;
uniform float emission;
uniform float bands;
// After rain: from dry to soaked.
uniform float wetness;
uniform vec3 eye;
out vec4 pixel;

float Shadowed() {
  vec3 toward = normalize(sun);
  float shade = 1.0;
  for (int i = 0; i < 64; ++i) {
    vec4 cloud = clouds[i];
    if (cloud.w <= 0.0) {
      continue;
    }
    // Where the line from here to the sun passes the ball's height.
    vec2 up = world.xz + toward.xz / max(toward.y, 0.2) * (cloud.z - world.y);
    float far = smoothstep(0.3 * cloud.w, 1.1 * cloud.w, length(up - cloud.xy));
    shade *= 1.0 - cloudShade * (1.0 - far);
  }
  return max(shade, 1.0 - cloudShade);
}

void main() {
  float sunlit = max(dot(normalize(normal), normalize(sun)), 0.0) * Shadowed();
  // Cel shading, like raylib's example: the sun's light in a few flat bands,
  // as a cartoon's colours are, where no bands is smooth.
  if (bands > 0.0) {
    sunlit = min(floor(sunlit * bands), bands - 1.0) / (bands - 1.0);
  }
  vec3 light = ambient + sunlight * sunlit + emission;
  vec3 lit = colDiffuse.rgb * color.rgb * light;
  // Wet, what faces up is darker, and the light shines off it, where it
  // comes back to the eye.
  vec3 facing = normalize(normal);
  float wet = wetness * smoothstep(0.55, 0.9, facing.y);
  vec3 reflected = reflect(-normalize(sun), facing);
  float shine = pow(max(dot(reflected, normalize(eye - world)), 0.0), 60.0);
  lit = lit * (1.0 - 0.38 * wet) + sunlight * shine * 0.45 * wet;
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

// In metres, so that an outline is thinner the further away it is. A
// figure's arms are about 12 centimetres across, so it's a third of that.
constexpr float kOutlineThickness = 0.04f;

constexpr std::array<float, 3> kPreviewBackground = {0.1f, 0.04f, 0.05f};

// The sun's light in three bands: in shadow, half lit and lit.
constexpr float kBands = 3.0f;

// A shadow reaches out from a figure's middle about as far as its shoulders
// are wide, in metres, and it's black, at about a third of the way to opaque.
constexpr float kShadowRadius = 0.42f;
// Around the monster the player picked, in the red of its nameplate.
constexpr float kRingInner = 0.55f;
constexpr float kRingOuter = 0.68f;
constexpr Color kRing = {235, 90, 75, 220};
// Where the player's walking to.
constexpr Color kDestination = {255, 255, 255, 200};
// A duel's flag, in gold, a pole a little taller than a player, and its
// furthest ring fainter, as it's there to be seen from inside.
constexpr Color kFlag = {255, 211, 77, 230};
constexpr Color kFlagEdge = {255, 211, 77, 110};
constexpr Vector3 kPole{0.12f, 2.4f, 0.12f};
// A monster's skill on its way, in the skill's colour: its whole shape faint,
// as much as has filled stronger, and its edge, to step out past, strongest.
constexpr float kTelegraphShape = 0.18f;
constexpr float kTelegraphFill = 0.4f;
constexpr float kTelegraphEdge = 0.85f;
// In metres: the edge's width, and the widest band a shape is drawn in, so
// that it lies on the ground.
constexpr float kTelegraphRim = 0.12f;
constexpr float kBand = 0.5f;
constexpr unsigned char kShadowDarkness = 82;

// Nothing casts a shadow, so each figure gets a soft dark patch under it,
// dark in the middle and fading out at the edge, with each corner on the
// ground, so it lies on a slope.
void DrawShadow(const Terrain& terrain, Vector3 at, float radius) {
  constexpr int kSides = 16;
  auto corner = [&](float distance, int i, unsigned char alpha) {
    const float turn = 2.0f * PI * i / kSides;
    const float x = at.x + distance * std::cos(turn);
    const float z = at.z + distance * std::sin(turn);
    rlColor4ub(0, 0, 0, alpha);
    // A little above it, so the ground doesn't cover it.
    rlVertex3f(x, terrain.Height(x, z) + 0.03f, z);
  };
  const float inner = 0.6f * radius;
  rlBegin(RL_TRIANGLES);
  for (int i = 0; i < kSides; ++i) {
    // The middle, as dark all over, then a ring that fades out.
    corner(0.0f, i, kShadowDarkness);
    corner(inner, i + 1, kShadowDarkness);
    corner(inner, i, kShadowDarkness);
    corner(inner, i, kShadowDarkness);
    corner(inner, i + 1, kShadowDarkness);
    corner(radius, i + 1, 0);
    corner(inner, i, kShadowDarkness);
    corner(radius, i + 1, 0);
    corner(radius, i, 0);
  }
  rlEnd();
}

// A ring at a figure's feet, which lies on a slope as a shadow does, or a
// slice of one, as wide as the arc, in radians, about the way the yaw faces.
// A wide one has more sides, a metre or so long, so that it follows the
// ground.
void DrawRing(const Terrain& terrain, Vector3 at, float inner, float outer,
              Color color, float yaw = 0.0f, float arc = 2.0f * PI) {
  const int sides = std::max(32, static_cast<int>(outer * arc));
  // The yaw goes round from z towards x, and the turns from x towards z.
  const float first = PI / 2.0f - yaw - arc / 2.0f;
  auto corner = [&](float distance, int i) {
    const float turn = first + arc * i / sides;
    const float x = at.x + distance * std::cos(turn);
    const float z = at.z + distance * std::sin(turn);
    rlColor4ub(color.r, color.g, color.b, color.a);
    // A little above the ground, so that the ground doesn't cover it.
    rlVertex3f(x, terrain.Height(x, z) + 0.04f, z);
  };
  rlBegin(RL_TRIANGLES);
  for (int i = 0; i < sides; ++i) {
    corner(inner, i);
    corner(outer, i + 1);
    corner(outer, i);
    corner(inner, i);
    corner(inner, i + 1);
    corner(outer, i + 1);
  }
  rlEnd();
}

// The shape of a monster's skill on its way: all of it faint, as much as has
// filled stronger, and its edges strongest.
void DrawTelegraph(const Terrain& terrain, const Telegraph& telegraph) {
  const monsters::Skill& skill = telegraph.skill;
  const Color color = GetColor(skill.color());
  const Vector3 at{telegraph.at.x, 0.0f, telegraph.at.z};
  // From the monster out, in bands, each a slice of a ring, so that a wide
  // shape lies on the ground too.
  const auto slice = [&](float inner, float outer, float alpha, float yaw,
                         float arc) {
    const int bands = static_cast<int>(std::ceil((outer - inner) / kBand));
    for (int band = 0; band < bands; ++band) {
      const float from = inner + static_cast<float>(band) * kBand;
      DrawRing(terrain, at, from, std::min(from + kBand, outer),
               Fade(color, alpha), yaw, arc);
    }
  };
  const float arc =
      skill.shape() == monsters::CONE ? skill.arc() * DEG2RAD : 2.0f * PI;
  const float size = skill.size();
  slice(0.0f, size, kTelegraphShape, telegraph.yaw, arc);
  slice(0.0f, size * telegraph.Filled(), kTelegraphFill, telegraph.yaw, arc);
  slice(size - kTelegraphRim, size, kTelegraphEdge, telegraph.yaw, arc);
  // A cone's sides are edges too, each a slice as narrow as the rim at the
  // end.
  if (skill.shape() == monsters::CONE) {
    const float side = kTelegraphRim / size;
    for (const float way : {-1.0f, 1.0f}) {
      slice(0.0f, size, kTelegraphEdge,
            telegraph.yaw + way * (arc - side) / 2.0f, side);
    }
  }
}

// The fog as far as it's lifted: from all of it within a couple of metres, to
// the atmosphere's own.
Atmosphere Lifted(Atmosphere atmosphere, float clear) {
  constexpr float kNearest = 2.0f;
  atmosphere.fog_near *= clear;
  atmosphere.fog_far = kNearest + (atmosphere.fog_far - kNearest) * clear;
  return atmosphere;
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
  SetShaderValue(shader, GetShaderLocation(shader, "wetness"),
                 &atmosphere.wetness, SHADER_UNIFORM_FLOAT);
}

}  // namespace

Renderer::Renderer(const Content& content)
    : cube_shader_(LoadGlsl(kVertexShader, kFragmentShader)),
      character_shader_(LoadGlsl(kSkinnedVertexShader, kFragmentShader)),
      outline_{.still = LoadGlsl(kOutlineVertexShader, kOutlineFragmentShader),
               .skinned = LoadGlsl(kSkinnedOutlineVertexShader,
                                   kOutlineFragmentShader)},
      cube_(LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f))),
      disc_(LoadModelFromMesh(Disc().Upload())),
      spire_(LoadModelFromMesh(Spire().Upload())),
      characters_(content.rig, character_shader_, content.figures),
      fx_(std::make_unique<Fx>(content.skills, cube_shader_, content.items)),
      sky_dome_(std::make_unique<Sky>()),
      weather_(std::make_unique<Weather>()) {
  for (Model* model : {&cube_, &disc_, &spire_}) {
    model->materials[0].shader = cube_shader_;
  }
  for (const Shader& shader : {outline_.still, outline_.skinned}) {
    SetShaderValue(shader, GetShaderLocation(shader, "thickness"),
                   &kOutlineThickness, SHADER_UNIFORM_FLOAT);
  }
  // Weapons have no bones, so they're drawn with the cube's shader.
  for (const auto& [id, item] : content.items.by_id) {
    characters_.Hold(id, item.look(), cube_shader_);
    characters_.Wear(id, item.look());
  }
  for (const auto& [id, skill] : content.skills.by_id) {
    characters_.Learn(id, skill.look());
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
  UnloadModel(disc_);
  UnloadModel(spire_);
  UnloadShader(character_shader_);
  UnloadShader(outline_.still);
  UnloadShader(outline_.skinned);
  UnloadShader(cube_shader_);
}

void Renderer::set_terrain(const Terrain& terrain, int land) {
  UnloadTerrain();
  terrain_ = terrain;
  land_ = land;
  const Look& look = kLooks[static_cast<std::size_t>(land) % kLooks.size()];
  atmosphere_ = look.atmosphere;
  weather_->set_land(terrain, land);
  // The ground and the trees have no bones either.
  chunks_ = LoadChunks(terrain, look, tree_, cube_shader_);
  sea_ = std::make_unique<Sea>(terrain, look);
  sky_dome_->Gather(terrain.seed());
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

void Renderer::Animate(float seconds) {
  fx_->Update(figures_, seconds);
  reactions_.Update(seconds);
  reactions_.Apply(figures_);
  fx_->Hold(figures_);
  clock_ += seconds;
  // Without the time of day, it's always noon for what glows.
  protocol::Sky shown = sky_.value_or(protocol::Sky());
  if (!sky_ || !showing_.day_night) {
    shown.set_day(0.5f);
  }
  weather_->Update(seconds, shown, sky_.has_value() && showing_.weather,
                   camera_);
  const Look& look = kLooks[static_cast<std::size_t>(land_) % kLooks.size()];
  atmosphere_ =
      sky_ ? AtmosphereUnder(land_, *sky_, showing_) : look.atmosphere;
  atmosphere_.wetness = weather_->wetness();
  // A bolt lights up the sky, and everything under it, for a moment.
  const float flash = weather_->flash();
  for (int c = 0; c < 3; ++c) {
    atmosphere_.sky[c] += flash * 0.55f;
    atmosphere_.zenith[c] += flash * 0.4f;
    atmosphere_.ambient[c] += flash * 0.45f;
    atmosphere_.clouds[c] += flash * 0.6f;
  }
  if (adjust_) {
    adjust_(atmosphere_);
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
  const Atmosphere atmosphere = Lifted(atmosphere_, clear_);
  SetAtmosphere(cube_shader_, atmosphere);
  SetAtmosphere(character_shader_, atmosphere);
  SetFog(outline_.still, atmosphere);
  SetFog(outline_.skinned, atmosphere);
  for (const Shader& shader : {cube_shader_, character_shader_}) {
    SetShaderValue(shader, GetShaderLocation(shader, "eye"), &camera_.position,
                   SHADER_UNIFORM_VEC3);
  }
  // A heavy blow nearby shakes the camera, and a skill of ours can take it
  // in closer, though never past where it looks.
  Camera3D camera = camera_;
  const Vector3 toward = Vector3Subtract(camera_.target, camera_.position);
  const float zoom = std::min(fx_->Zoom(), 0.6f * Vector3Length(toward));
  camera.position =
      Vector3Add(camera.position, Vector3Scale(Vector3Normalize(toward), zoom));
  const Vector3 shake = fx_->Shake(camera_.position);
  camera.position = Vector3Add(camera.position, shake);
  camera.target = Vector3Add(camera.target, shake);
  if (terrain_) {
    sky_dome_->Shade(cube_shader_, camera, atmosphere, clock_);
    sky_dome_->Shade(character_shader_, camera, atmosphere, clock_);
  }
  BeginMode3D(camera);
  // First, as it's behind everything.
  if (terrain_) {
    sky_dome_->Draw(camera, atmosphere, clock_);
  }
  // As BeginMode3D set them up, so we cull with what the GPU will use.
  const Matrix view_projection =
      MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
  for (const Chunk& chunk : chunks_) {
    if (InSight(chunk.box, camera_.position, view_projection,
                atmosphere.fog_far)) {
      DrawCounted(chunk.model.meshes[0], chunk.model.materials[0],
                  MatrixIdentity());
    }
  }
  for (const Cube& cube : cubes_) {
    const Model& model = cube.shape == Shape::kDisc    ? disc_
                         : cube.shape == Shape::kSpire ? spire_
                                                       : cube_;
    DrawModelEx(model, cube.position, {0.0f, 1.0f, 0.0f}, 0.0f, cube.size,
                cube.color);
  }
  if (flag_) {
    DrawModelEx(spire_, {flag_->x, flag_->y + kPole.y / 2.0f, flag_->z},
                {0.0f, 1.0f, 0.0f}, 0.0f, kPole, kFlag);
  }
  for (const Figure& figure : figures_) {
    DrawFigure(figure);
  }
  if (terrain_) {
    fx_->DrawSolid(*terrain_);
  }
  // The shadows are see-through, and lie on the ground, so they go after
  // what's solid, without hiding what's behind them from what comes next.
  if (terrain_) {
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ALPHA);
    for (const Figure& figure : figures_) {
      DrawShadow(*terrain_, figure.position, kShadowRadius);
    }
    for (const auto& [monster, telegraph] : telegraphs_) {
      DrawTelegraph(*terrain_, telegraph);
    }
    // After all the shadows, so that none of them, such as a player's
    // beside the monster, darkens the ring.
    for (const Figure& figure : figures_) {
      if (Picked(figure, target_, target_player_)) {
        DrawRing(*terrain_, figure.position, kRingInner, kRingOuter, kRing);
      }
    }
    if (destination_) {
      DrawRing(*terrain_, *destination_, kRingInner * 0.6f, kRingOuter * 0.6f,
               kDestination);
    }
    if (flag_) {
      DrawRing(*terrain_, *flag_, kRingInner, kRingOuter, kFlag);
      DrawRing(*terrain_, *flag_, Duels::kFlagReach - 0.2f, Duels::kFlagReach,
               kFlagEdge);
    }
    // raylib batches these, and alpha is its blend already, so ending it
    // doesn't send them. They go now, while the depth mask's off, or a
    // telegraph's fill would fight the shape under it for each pixel.
    rlDrawRenderBatchActive();
    EndBlendMode();
    rlEnableDepthMask();
  }
  // Last, as we see the ground under it through the water.
  if (sea_) {
    sea_->Draw(camera_, atmosphere, cel_shading_ ? kBands : 0.0f, clock_);
    ++work_.draws;
    work_.triangles += sea_->triangles();
  }
  // Over the sea too, as the rain falls into it.
  weather_->Draw(camera, atmosphere);
  // See-through, like the light, but lit as the figures are.
  fx_->DrawGhosts(characters_);
  // The skills' light goes last, over everything it lights up.
  if (terrain_) {
    fx_->DrawLight(camera, *terrain_, atmosphere, characters_);
  }
  EndMode3D();
}

void Renderer::DrawPreview(const Figure& figure, const Camera3D& camera) const {
  // Clear, so that the interface's own background shows round the figure.
  ClearBackground(
      ColorFromNormalized({kPreviewBackground[0], kPreviewBackground[1],
                           kPreviewBackground[2], 0.0f}));
  // Lit like the Meadows, wherever we are.
  Atmosphere preview = kMeadows.atmosphere;
  preview.sky = kPreviewBackground;
  SetAtmosphere(cube_shader_, preview);
  SetAtmosphere(character_shader_, preview);
  SetFog(outline_.still, preview);
  SetFog(outline_.skinned, preview);
  // Nor under the land's clouds, its body or what it holds.
  constexpr float kNoShade = 0.0f;
  for (const Shader& shader : {cube_shader_, character_shader_}) {
    SetShaderValue(shader, GetShaderLocation(shader, "cloudShade"), &kNoShade,
                   SHADER_UNIFORM_FLOAT);
  }
  BeginMode3D(camera);
  DrawFigure(figure);
  EndMode3D();
}

std::optional<Gait> Renderer::GaitOf(const Figure& figure) const {
  if (figure.monster) {
    const auto kind = monsters_.find(figure.kind);
    return kind != monsters_.end() ? kind->second->GaitOf(figure)
                                   : std::nullopt;
  }
  return characters_.GaitOf(figure);
}

void Renderer::DrawFigure(const Figure& figure) const {
  const Characters* looks = &characters_;
  if (figure.monster) {
    const auto kind = monsters_.find(figure.kind);
    looks = kind != monsters_.end() ? kind->second.get() : nullptr;
  }
  Color color = figure.color;
  if (!figure.monster && !figure.npc) {
    color = figure.features && figure.features->shirt
                ? ShirtColor(*figure.features->shirt)
                : PlayerColor(figure.id);
  }
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
