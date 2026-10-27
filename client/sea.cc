#include "client/sea.h"

#include <raymath.h>

#include <cmath>

#include "client/triangles.h"
#include "terrain/terrain.h"

namespace nanommo {

namespace {

// Waves: three of them, each its own way and length, raise and lower each
// corner of the sea, and their slopes there tilt its normal.
constexpr const char* kVertexShader = R"(#version 330
in vec3 vertexPosition;
uniform mat4 matModel;
uniform mat4 matView;
uniform mat4 matProjection;
uniform vec3 eye;
uniform float seconds;
out vec3 world;
out vec3 normal;
out float depth;

const vec2 kWays[3] = vec2[](vec2(1.0, 0.35), vec2(-0.45, 1.0),
                             vec2(0.6, -0.8));
// In metres, from the top of a wave to the next.
const float kLengths[3] = float[](10.0, 7.1, 5.3);
// In metres, from the still sea to the top of a wave.
const float kHeight = 0.45;

void main() {
  world = (matModel * vec4(vertexPosition, 1.0)).xyz;
  // Far away, a cell is a few pixels, and waves that small only flicker.
  float calm = 1.0 - smoothstep(40.0, 100.0, distance(world.xz, eye.xz));
  float height = 0.0;
  vec2 slope = vec2(0.0);
  for (int i = 0; i < 3; ++i) {
    vec2 way = normalize(kWays[i]);
    float k = 6.2831853 / kLengths[i];
    // Each moves as fast as a wave of its length does at sea.
    float phase = dot(way, world.xz) * k + seconds * sqrt(9.8 * k);
    height += sin(phase) / 3.0;
    slope += way * k * cos(phase) / 3.0;
  }
  world.y += kHeight * calm * height;
  slope *= kHeight * calm;
  normal = vec3(-slope.x, 1.0, -slope.y);
  gl_Position = matProjection * matView * vec4(world, 1.0);
  depth = gl_Position.w;
}
)";

constexpr const char* kFragmentShader = R"(#version 330
in vec3 world;
in vec3 normal;
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
uniform vec3 eye;
uniform float reach;
out vec4 pixel;

// How much of the sun it reflects to the eye.
const float kGlint = 0.6;

void main() {
  vec3 surface = normalize(normal);
  float sunlit = max(dot(surface, normalize(sun)), 0.0);
  if (bands > 0.0) {
    sunlit = min(floor(sunlit * bands), bands - 1.0) / (bands - 1.0);
  }
  vec3 light = ambient + sunlight * sunlit + emission;
  // The sun, reflected off the surface, where it comes back to the eye.
  vec3 reflected = reflect(-normalize(sun), surface);
  float glint = pow(max(dot(reflected, normalize(eye - world)), 0.0), 24.0);
  vec3 lit = colDiffuse.rgb * light + sunlight * glint * kGlint;
  float fog = clamp((depth - fogNear) / (fogFar - fogNear), 0.0, 1.0);
  // Into the sky towards the edge of the sea, wherever the fog ends.
  fog = max(fog, smoothstep(reach - 30.0, reach, distance(world.xz, eye.xz)));
  pixel = vec4(mix(lit, fogColor, fog), colDiffuse.a);
}
)";

// In metres from the camera to each side of the sea, a little past the fog.
constexpr float kReach = 160.0f;

void Set(Shader shader, const char* name, const void* value, int type) {
  SetShaderValue(shader, GetShaderLocation(shader, name), value, type);
}

}  // namespace

Sea::Sea(const Look& look)
    : shader_(LoadShaderFromMemory(kVertexShader, kFragmentShader)) {
  // A square around the camera, cut into the ground's cells, two triangles
  // each, which the waves move.
  const int cells = static_cast<int>(2.0f * kReach / Terrain::kCell);
  const auto corner = [](int i, int j) {
    return Vector3{i * Terrain::kCell - kReach, 0.0f,
                   j * Terrain::kCell - kReach};
  };
  Triangles grid;
  for (int j = 0; j < cells; ++j) {
    for (int i = 0; i < cells; ++i) {
      grid.Add(corner(i, j), corner(i + 1, j + 1), corner(i + 1, j), WHITE);
      grid.Add(corner(i, j), corner(i, j + 1), corner(i + 1, j + 1), WHITE);
    }
  }
  model_ = LoadModelFromMesh(grid.Upload());
  model_.materials[0].shader = shader_;
  model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = look.sea;
  Set(shader_, "emission", &look.sea_emission, SHADER_UNIFORM_FLOAT);
  Set(shader_, "reach", &kReach, SHADER_UNIFORM_FLOAT);
}

// UnloadModel leaves the model's shader alone.
Sea::~Sea() {
  UnloadModel(model_);
  UnloadShader(shader_);
}

void Sea::Draw(const Camera3D& camera, const Atmosphere& atmosphere,
               float bands, float seconds) const {
  Set(shader_, "sun", atmosphere.sun.data(), SHADER_UNIFORM_VEC3);
  Set(shader_, "sunlight", atmosphere.sunlight.data(), SHADER_UNIFORM_VEC3);
  Set(shader_, "ambient", atmosphere.ambient.data(), SHADER_UNIFORM_VEC3);
  Set(shader_, "fogColor", atmosphere.sky.data(), SHADER_UNIFORM_VEC3);
  Set(shader_, "fogNear", &atmosphere.fog_near, SHADER_UNIFORM_FLOAT);
  Set(shader_, "fogFar", &atmosphere.fog_far, SHADER_UNIFORM_FLOAT);
  Set(shader_, "bands", &bands, SHADER_UNIFORM_FLOAT);
  Set(shader_, "eye", &camera.position, SHADER_UNIFORM_VEC3);
  Set(shader_, "seconds", &seconds, SHADER_UNIFORM_FLOAT);
  // It follows the camera a whole cell at a time, so that each corner of it
  // stays where a corner of the ground is, and the waves, which go by where
  // they are in the world, don't move with it.
  const float x =
      std::round(camera.position.x / Terrain::kCell) * Terrain::kCell;
  const float z =
      std::round(camera.position.z / Terrain::kCell) * Terrain::kCell;
  DrawMesh(model_.meshes[0], model_.materials[0],
           MatrixTranslate(x, Terrain::kSeaLevel, z));
}

}  // namespace nanommo
