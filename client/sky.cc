#include "client/sky.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace nanommo {
namespace {

// The sky is a ball around the camera, behind everything else, and inside
// what the camera can see: raylib draws nothing past 1000 metres.
constexpr float kDome = 800.0f;
// In metres above the sea.
constexpr float kLowest = 80.0f;
constexpr float kHighest = 130.0f;
// How much of the sun's light a cloud's shadow takes away.
constexpr float kShade = 0.45f;

// Its colour from the horizon up, then the glow around the sun, and the sun.
constexpr const char* kDomeVertexShader = R"(#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
out vec3 direction;

void main() {
  direction = vertexPosition;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

constexpr const char* kDomeFragmentShader = R"(#version 330
in vec3 direction;
uniform vec3 horizon;
uniform vec3 zenith;
uniform vec3 sun;
uniform vec3 disc;
uniform float edge;
out vec4 pixel;

void main() {
  vec3 look = normalize(direction);
  vec3 toward = normalize(sun);
  vec3 color = mix(horizon, zenith, smoothstep(0.0, 0.6, max(look.y, 0.0)));
  float facing = max(dot(look, toward), 0.0);
  color += disc * (0.06 * pow(facing, 48.0) + 0.02 * pow(facing, 6.0));
  // The disc, with its edge smoothed over a few pixels' worth of angle.
  float inside = smoothstep(edge - 0.00005, edge + 0.00005, dot(look, toward));
  pixel = vec4(mix(color, disc, inside), 1.0);
}
)";

// Lit by the sun face by face, from the faces' own slopes, as everything else
// in the world is low-poly too, and fading into the horizon far away.
constexpr const char* kCloudVertexShader = R"(#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
uniform mat4 matModel;
out vec3 world;

void main() {
  world = (matModel * vec4(vertexPosition, 1.0)).xyz;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

constexpr const char* kCloudFragmentShader = R"(#version 330
in vec3 world;
uniform vec3 eye;
uniform vec3 sun;
uniform vec3 clouds;
uniform vec3 horizon;
uniform float fadeNear;
uniform float fadeFar;
out vec4 pixel;

void main() {
  // A face too small for a pixel has no slope to speak of, and normalizing
  // nothing gives NaN, which the bloom would spread over the whole picture.
  vec3 across = cross(dFdx(world), dFdy(world));
  float size = length(across);
  vec3 normal = size > 1e-10 ? across / size : vec3(0.0, 1.0, 0.0);
  float sunlit = max(dot(normal, normalize(sun)), 0.0);
  vec3 color = clouds * (0.62 + 0.1 * normal.y + 0.42 * sunlit);
  float far = smoothstep(fadeNear, fadeFar, length(world.xz - eye.xz));
  pixel = vec4(mix(color, horizon, far), 1.0);
}
)";

// The same numbers on every machine, from the seed.
class Random {
 public:
  explicit Random(uint32_t seed) : state_(seed) {}

  float Between(float low, float high) {
    state_ += 0x9E3779B9u;
    uint32_t z = state_;
    z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
    z = (z ^ (z >> 13)) * 0xC2B2AE35u;
    z ^= z >> 16;
    return low + (high - low) * static_cast<float>(z >> 8) / 16777216.0f;
  }

 private:
  uint32_t state_;
};

void SetVec3(Shader shader, const char* name, const std::array<float, 3>& v) {
  SetShaderValue(shader, GetShaderLocation(shader, name), v.data(),
                 SHADER_UNIFORM_VEC3);
}

void SetFloat(Shader shader, const char* name, float value) {
  SetShaderValue(shader, GetShaderLocation(shader, name), &value,
                 SHADER_UNIFORM_FLOAT);
}

}  // namespace

Sky::Sky()
    : dome_shader_(
          LoadShaderFromMemory(kDomeVertexShader, kDomeFragmentShader)),
      cloud_shader_(
          LoadShaderFromMemory(kCloudVertexShader, kCloudFragmentShader)),
      dome_(LoadModelFromMesh(GenMeshSphere(1.0f, 24, 32))),
      // Few rings and slices, so that each ball is made of big flat faces.
      puff_(LoadModelFromMesh(GenMeshSphere(1.0f, 5, 7))) {
  dome_.materials[0].shader = dome_shader_;
  puff_.materials[0].shader = cloud_shader_;
}

Sky::~Sky() {
  UnloadModel(dome_);
  UnloadModel(puff_);
  UnloadShader(dome_shader_);
  UnloadShader(cloud_shader_);
}

void Sky::Gather(uint32_t seed) {
  Random random(seed * 7919u + 17u);
  clouds_.clear();
  for (int i = 0; i < kClouds; ++i) {
    Cloud cloud;
    cloud.at = {random.Between(-kSpread / 2, kSpread / 2),
                random.Between(-kSpread / 2, kSpread / 2)};
    cloud.height = random.Between(kLowest, kHighest);
    cloud.rank = random.Between(0.0f, 1.0f);
    // A row of balls along a line, biggest in the middle, turned any way.
    const float length = random.Between(40.0f, 95.0f);
    const int puffs = static_cast<int>(random.Between(4.0f, 8.0f));
    const float turn = random.Between(0.0f, 2.0f * PI);
    const Vector2 along = {std::cos(turn), std::sin(turn)};
    for (int p = 0; p < puffs; ++p) {
      const float share = (p + 0.5f) / puffs - 0.5f;
      const float middle = 1.0f - 1.4f * std::abs(share);
      const float size = length * random.Between(0.22f, 0.3f) * middle;
      const float x = share * length + random.Between(-0.1f, 0.1f) * length;
      const float z = random.Between(-0.12f, 0.12f) * length;
      cloud.puffs.push_back(
          {.offset = {along.x * x - along.y * z, size * 0.2f,
                      along.y * x + along.x * z},
           .size = {size, size * random.Between(0.5f, 0.65f), size}});
    }
    clouds_.push_back(std::move(cloud));
  }
}

Vector2 Sky::Blown(const Cloud& cloud, const Camera3D& camera,
                   float seconds) const {
  const Vector2 at = Vector2Add(cloud.at, Vector2Scale(kWind, seconds));
  const auto wrap = [](float d) {
    return d - kSpread * std::floor((d + kSpread / 2) / kSpread);
  };
  return {camera.position.x + wrap(at.x - camera.position.x),
          camera.position.z + wrap(at.y - camera.position.z)};
}

void Sky::Draw(const Camera3D& camera, const Atmosphere& atmosphere,
               float seconds) const {
  SetVec3(dome_shader_, "horizon", atmosphere.sky);
  SetVec3(dome_shader_, "zenith", atmosphere.zenith);
  SetVec3(dome_shader_, "sun", atmosphere.sun);
  SetVec3(dome_shader_, "disc", atmosphere.disc);
  SetFloat(dome_shader_, "edge",
           std::cos(atmosphere.disc_size * 0.5f * DEG2RAD));
  // From inside, behind everything, so it hides nothing.
  rlDisableBackfaceCulling();
  rlDisableDepthMask();
  DrawModelEx(dome_, camera.position, {0.0f, 1.0f, 0.0f}, 0.0f,
              {kDome, kDome, kDome}, WHITE);
  rlEnableDepthMask();
  rlEnableBackfaceCulling();

  SetVec3(cloud_shader_, "eye",
          {camera.position.x, camera.position.y, camera.position.z});
  SetVec3(cloud_shader_, "sun", atmosphere.sun);
  SetVec3(cloud_shader_, "clouds", atmosphere.clouds);
  SetVec3(cloud_shader_, "horizon", atmosphere.sky);
  // Gone into the horizon before they reach the square's edge, where they
  // jump to the other side.
  SetFloat(cloud_shader_, "fadeNear", 0.28f * kSpread);
  SetFloat(cloud_shader_, "fadeFar", 0.48f * kSpread);
  for (const Cloud& cloud : clouds_) {
    if (cloud.rank >= atmosphere.cloudiness) {
      continue;
    }
    const Vector2 at = Blown(cloud, camera, seconds);
    for (const Puff& puff : cloud.puffs) {
      DrawModelEx(puff_,
                  {at.x + puff.offset.x, cloud.height + puff.offset.y,
                   at.y + puff.offset.z},
                  {0.0f, 1.0f, 0.0f}, 0.0f, puff.size, WHITE);
    }
  }
}

void Sky::Shade(Shader shader, const Camera3D& camera,
                const Atmosphere& atmosphere, float seconds) const {
  std::vector<Vector4> balls;
  for (const Cloud& cloud : clouds_) {
    if (cloud.rank >= atmosphere.cloudiness) {
      continue;
    }
    const Vector2 at = Blown(cloud, camera, seconds);
    for (const Puff& puff : cloud.puffs) {
      balls.push_back({at.x + puff.offset.x, at.y + puff.offset.z,
                       cloud.height + puff.offset.y, puff.size.x});
    }
  }
  // Nearest first, to what the camera's looking at.
  const auto away = [&](const Vector4& ball) {
    return Vector2Distance({ball.x, ball.y},
                           {camera.target.x, camera.target.z});
  };
  std::ranges::sort(balls, {}, away);
  std::array<Vector4, kShadows> shadows{};
  std::copy_n(balls.begin(), std::min<size_t>(balls.size(), kShadows),
              shadows.begin());
  SetShaderValueV(shader, GetShaderLocation(shader, "clouds"), shadows.data(),
                  SHADER_UNIFORM_VEC4, kShadows);
  SetFloat(shader, "cloudShade", kShade);
}

}  // namespace nanommo
