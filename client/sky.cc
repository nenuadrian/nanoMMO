#include "client/sky.h"

#include <rlgl.h>

#include <array>
#include <cmath>

namespace nanommo {
namespace {

// The sky is a ball around the camera, behind everything else, and inside
// what the camera can see: raylib draws nothing past 1000 metres.
constexpr float kDome = 800.0f;

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
      dome_(LoadModelFromMesh(GenMeshSphere(1.0f, 24, 32))) {
  dome_.materials[0].shader = dome_shader_;
}

Sky::~Sky() {
  UnloadModel(dome_);
  UnloadShader(dome_shader_);
}

void Sky::Draw(const Camera3D& camera, const Atmosphere& atmosphere) const {
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
}

}  // namespace nanommo
