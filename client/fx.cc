#include "client/fx.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace nanommo {

namespace {

constexpr const char* kLightVertexShader = R"(#version 330
in vec3 vertexPosition;
in vec4 vertexColor;
uniform mat4 mvp;
out vec4 color;
out float depth;

void main() {
  color = vertexColor;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
  depth = gl_Position.w;
}
)";

// Light is brighter than white, so that it blooms, and fades into the fog.
constexpr const char* kLightFragmentShader = R"(#version 330
in vec4 color;
in float depth;
uniform float fogNear;
uniform float fogFar;
out vec4 pixel;

const float kBright = 3.0;

void main() {
  float fog = clamp((depth - fogNear) / (fogFar - fogNear), 0.0, 1.0);
  pixel = vec4(color.rgb * kBright, color.a * (1.0 - fog));
}
)";

// How much of the blade's path a trail shows, in seconds.
constexpr float kTrail = 0.16f;
constexpr Vector3 kUp{0.0f, 1.0f, 0.0f};
// Where a ball of light is, above someone's feet: about their middle.
constexpr float kMiddle = 0.9f;

float EaseOut(float t) { return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); }

// Towards white as it's hotter, from 0 to 1.
Color Hot(Color color, float heat) {
  return ColorLerp(color, WHITE, std::clamp(heat, 0.0f, 1.0f));
}

void Vertex(Vector3 at, Color color, float alpha) {
  rlColor4ub(color.r, color.g, color.b,
             static_cast<unsigned char>(std::clamp(alpha, 0.0f, 1.0f) *
                                        static_cast<float>(color.a)));
  rlVertex3f(at.x, at.y, at.z);
}

// Two triangles from a to b on one side, and c to d on the other.
void Quad(Vector3 a, Color a_color, float a_alpha, Vector3 b, Color b_color,
          float b_alpha, Vector3 c, Color c_color, float c_alpha, Vector3 d,
          Color d_color, float d_alpha) {
  Vertex(a, a_color, a_alpha);
  Vertex(b, b_color, b_alpha);
  Vertex(d, d_color, d_alpha);
  Vertex(a, a_color, a_alpha);
  Vertex(d, d_color, d_alpha);
  Vertex(c, c_color, c_alpha);
}

// A round patch of light facing the camera: a bright middle, which blooms,
// fading fast to nothing at its edge.
void Blob(Vector3 at, float radius, Color color, float alpha,
          const Camera3D& camera) {
  constexpr int kSides = 20;
  constexpr float kCore = 0.35f;
  const Vector3 forward =
      Vector3Normalize(Vector3Subtract(camera.target, camera.position));
  const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, kUp));
  const Vector3 up = Vector3CrossProduct(right, forward);
  auto edge = [&](int j, float out) {
    const float turn = 2.0f * PI * static_cast<float>(j) / kSides;
    return Vector3Add(
        at, Vector3Add(Vector3Scale(right, out * radius * std::cos(turn)),
                       Vector3Scale(up, out * radius * std::sin(turn))));
  };
  const Color hot = Hot(color, 0.7f);
  for (int i = 0; i < kSides; ++i) {
    Vertex(at, hot, 0.85f * alpha);
    Vertex(edge(i, kCore), color, 0.55f * alpha);
    Vertex(edge(i + 1, kCore), color, 0.55f * alpha);
    Quad(edge(i, kCore), color, 0.55f * alpha, edge(i + 1, kCore), color,
         0.55f * alpha, edge(i, 1.0f), color, 0.0f, edge(i + 1, 1.0f), color,
         0.0f);
  }
}

}  // namespace

Fx::Fx(const SkillTable& skills)
    : skills_(skills.by_id),
      light_(LoadShaderFromMemory(kLightVertexShader, kLightFragmentShader)) {
  for (const auto& [id, skill] : skills_) {
    float end = 0.0f;
    for (const skills::Fx& fx : skill.look().fx()) {
      // A trail fades for a moment after the blade's stopped.
      end = std::max(
          end,
          static_cast<float>(fx.from_ms() + fx.for_ms()) / 1000.0f + kTrail);
    }
    ends_[id] = end;
  }
}

Fx::~Fx() { UnloadShader(light_); }

void Fx::Update(const std::vector<Figure>& figures, float seconds) {
  for (auto& [id, cast] : casts_) {
    cast.time += seconds;
  }
  for (const Figure& figure : figures) {
    const auto skill = skills_.find(figure.to_skill);
    if (figure.monster || figure.npc || figure.to != protocol::MOTION_SKILL ||
        skill == skills_.end()) {
      continue;
    }
    Cast& cast = casts_[figure.id];
    cast.figure = figure;
    cast.time = figure.to_time;
  }
  std::erase_if(casts_, [&](const auto& entry) {
    const Cast& cast = entry.second;
    return cast.time > ends_.at(cast.figure.to_skill);
  });
}

namespace {

// One fx of a cast, as far as it's got.
struct Moment {
  const skills::Fx& fx;
  /// Where it is.
  Vector3 at;
  /// Seconds since it started, and how long it lasts.
  float age;
  float span;
  Color color;

  float done() const { return age / span; }
};

// The blade, posed again at moments just gone, which makes a ribbon through
// where it's been, brightest at its tip.
void DrawTrail(const Figure& figure, float time, float from, float span,
               Color color, const Characters& characters) {
  constexpr int kSteps = 16;
  const float newest = std::min(time, from + span);
  const float oldest = std::max(from, time - kTrail);
  if (newest <= oldest) {
    return;
  }
  Figure posed = figure;
  posed.blend = 1.0f;
  std::array<std::pair<Vector3, Vector3>, kSteps + 1> blade{};
  std::array<float, kSteps + 1> fade{};
  for (int i = 0; i <= kSteps; ++i) {
    posed.to_time = oldest + (newest - oldest) * static_cast<float>(i) / kSteps;
    const auto found = characters.Blade(posed);
    if (!found) {
      return;
    }
    blade[i] = *found;
    fade[i] = std::pow(1.0f - (time - posed.to_time) / kTrail, 2.0f);
  }
  rlBegin(RL_TRIANGLES);
  for (int i = 0; i < kSteps; ++i) {
    const auto& [hilt, tip] = blade[i];
    const auto& [next_hilt, next_tip] = blade[i + 1];
    const Vector3 middle = Vector3Lerp(hilt, tip, 0.6f);
    const Vector3 next_middle = Vector3Lerp(next_hilt, next_tip, 0.6f);
    // From nothing at the hilt, to half way along, to bright at the tip.
    Quad(hilt, color, 0.0f, next_hilt, color, 0.0f, middle, color,
         0.5f * fade[i], next_middle, color, 0.5f * fade[i + 1]);
    Quad(middle, color, 0.5f * fade[i], next_middle, color, 0.5f * fade[i + 1],
         tip, Hot(color, fade[i]), fade[i], next_tip, Hot(color, fade[i + 1]),
         fade[i + 1]);
  }
  rlEnd();
}

// Grows fast and fades slowly.
void DrawGlow(const Moment& m, const Camera3D& camera, bool lifted) {
  const float u = m.done();
  const float radius =
      m.fx.size() * (0.5f + 0.5f * EaseOut(std::min(3.0f * u, 1.0f)));
  const float alpha =
      u < 0.15f ? u / 0.15f : std::pow((1.0f - u) / 0.85f, 2.0f);
  const Vector3 at =
      lifted ? Vector3Add(m.at, Vector3Scale(kUp, kMiddle)) : m.at;
  Blob(at, radius, m.color, alpha, camera);
}

}  // namespace

void Fx::DrawLight(const Camera3D& camera, const Atmosphere& atmosphere,
                   const Characters& characters) const {
  if (casts_.empty()) {
    return;
  }
  SetShaderValue(light_, GetShaderLocation(light_, "fogNear"),
                 &atmosphere.fog_near, SHADER_UNIFORM_FLOAT);
  SetShaderValue(light_, GetShaderLocation(light_, "fogFar"),
                 &atmosphere.fog_far, SHADER_UNIFORM_FLOAT);
  // Light goes over what's behind it, in its own colour, so it's drawn last,
  // without hiding anything, and from both sides.
  rlDrawRenderBatchActive();
  rlDisableBackfaceCulling();
  rlDisableDepthMask();
  BeginBlendMode(BLEND_ALPHA);
  BeginShaderMode(light_);
  for (const auto& [id, cast] : casts_) {
    const skills::Skill& skill = skills_.at(cast.figure.to_skill);
    for (int index = 0; index < skill.look().fx_size(); ++index) {
      const skills::Fx& fx = skill.look().fx(index);
      const float from = static_cast<float>(fx.from_ms()) / 1000.0f;
      const float span = static_cast<float>(fx.for_ms()) / 1000.0f;
      const float age = cast.time - from;
      const Color color = GetColor(fx.color());
      if (fx.kind() == skills::Fx::TRAIL) {
        DrawTrail(cast.figure, cast.time, from, span, color, characters);
        continue;
      }
      if (age < 0.0f || age > span) {
        continue;
      }
      Vector3 at = cast.figure.position;
      if (fx.at() == skills::Fx::BLADE) {
        Figure posed = cast.figure;
        posed.blend = 1.0f;
        if (const auto blade = characters.Blade(posed)) {
          at = blade->second;
        }
      }
      const Moment m{
          .fx = fx, .at = at, .age = age, .span = span, .color = color};
      rlBegin(RL_TRIANGLES);
      switch (fx.kind()) {
        case skills::Fx::GLOW:
          DrawGlow(m, camera, fx.at() != skills::Fx::BLADE);
          break;
        default:
          break;
      }
      rlEnd();
    }
  }
  EndShaderMode();
  EndBlendMode();
  rlEnableDepthMask();
  rlEnableBackfaceCulling();
}

}  // namespace nanommo
