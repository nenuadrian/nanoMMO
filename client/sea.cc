#include "client/sea.h"

#include <raymath.h>

namespace nanommo {

namespace {

// Where each pixel is in the world goes on to the fragment shader, which works
// out the ripples there.
constexpr const char* kVertexShader = R"(#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
uniform mat4 matModel;
out vec3 world;
out float depth;

void main() {
  world = (matModel * vec4(vertexPosition, 1.0)).xyz;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
  depth = gl_Position.w;
}
)";

// Ripples: three waves, each its own way and length, tilt the surface that
// each pixel lights, while the sea itself stays flat.
constexpr const char* kFragmentShader = R"(#version 330
in vec3 world;
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
uniform float seconds;
out vec4 pixel;

const vec2 kWays[3] = vec2[](vec2(1.0, 0.35), vec2(-0.45, 1.0),
                             vec2(0.6, -0.8));
// In metres, from the top of a wave to the next.
const float kLengths[3] = float[](1.5, 1.1, 0.8);
// The steepest the ripples tilt the surface.
const float kTilt = 0.3;
// How much of the sun it reflects to the eye.
const float kGlint = 0.6;

void main() {
  vec2 slope = vec2(0.0);
  for (int i = 0; i < 3; ++i) {
    vec2 way = normalize(kWays[i]);
    float k = 6.2831853 / kLengths[i];
    // Each moves as fast as a wave of its length does at sea.
    float phase = dot(way, world.xz) * k + seconds * sqrt(9.8 * k);
    slope += way * cos(phase) / 3.0;
  }
  vec3 normal = normalize(vec3(-kTilt * slope.x, 1.0, -kTilt * slope.y));
  float sunlit = max(dot(normal, normalize(sun)), 0.0);
  if (bands > 0.0) {
    sunlit = min(floor(sunlit * bands), bands - 1.0) / (bands - 1.0);
  }
  vec3 light = ambient + sunlight * sunlit + emission;
  // The sun, reflected off the surface, where it comes back to the eye.
  vec3 reflected = reflect(-normalize(sun), normal);
  float glint = pow(max(dot(reflected, normalize(eye - world)), 0.0), 24.0);
  vec3 lit = colDiffuse.rgb * light + sunlight * glint * kGlint;
  float fog = clamp((depth - fogNear) / (fogFar - fogNear), 0.0, 1.0);
  pixel = vec4(mix(lit, fogColor, fog), colDiffuse.a);
}
)";

void Set(Shader shader, const char* name, const void* value, int type) {
  SetShaderValue(shader, GetShaderLocation(shader, name), value, type);
}

}  // namespace

Sea::Sea(const Terrain& terrain, const Look& look)
    : shader_(LoadShaderFromMemory(kVertexShader, kFragmentShader)) {
  // A square at sea level, four times as wide as the terrain, so that the sea
  // goes on past it.
  const float size = 8.0f * terrain.half_size();
  model_ = LoadModelFromMesh(GenMeshPlane(size, size, 1, 1));
  model_.materials[0].shader = shader_;
  model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = look.sea;
  Set(shader_, "emission", &look.sea_emission, SHADER_UNIFORM_FLOAT);
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
  DrawMesh(model_.meshes[0], model_.materials[0],
           MatrixTranslate(0.0f, Terrain::kSeaLevel, 0.0f));
}

}  // namespace nanommo
