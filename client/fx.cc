#include "client/fx.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include "client/triangles.h"

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
constexpr float kGravity = 9.8f;
constexpr Vector3 kUp{0.0f, 1.0f, 0.0f};
// Where a ball of light is, above someone's feet: about their middle.
constexpr float kMiddle = 0.9f;

// The same number in [0, 1) for the same four, every time, so that a spark
// flies the same way in every frame.
float Random(uint32_t seed, uint32_t fx, uint32_t i, uint32_t what) {
  uint32_t h = seed * 0x9E3779B1u ^ (fx + 1) * 0x85EBCA77u ^
               (i + 1) * 0xC2B2AE3Du ^ (what + 1) * 0x27D4EB2Fu;
  h ^= h >> 15;
  h *= 0x2C1B3C6Du;
  h ^= h >> 12;
  h *= 0x297A2D39u;
  h ^= h >> 15;
  return static_cast<float>(h >> 8) / 16777216.0f;
}

float EaseOut(float t) { return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); }

// Past the end, and back.
float Overshoot(float t) {
  constexpr float kBack = 1.7f;
  const float s = t - 1.0f;
  return 1.0f + s * s * ((kBack + 1.0f) * s + kBack);
}

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

// A line from a to b, as wide as `width`, turned to face the camera, bright
// down its middle.
void Streak(Vector3 a, Vector3 b, float width, Color color, float alpha,
            Vector3 eye) {
  const Vector3 side =
      Vector3Scale(Vector3Normalize(Vector3CrossProduct(
                       Vector3Subtract(b, a), Vector3Subtract(eye, a))),
                   width / 2.0f);
  Quad(Vector3Subtract(a, side), color, 0.0f, Vector3Subtract(b, side), color,
       0.0f, a, color, alpha, b, color, alpha);
  Quad(a, color, alpha, b, color, alpha, Vector3Add(a, side), color, 0.0f,
       Vector3Add(b, side), color, 0.0f);
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

// On the ground, at a distance and a turn from somewhere.
Vector3 Around(const Terrain& terrain, Vector3 at, float distance, float turn,
               float above) {
  const float x = at.x + distance * std::sin(turn);
  const float z = at.z + distance * std::cos(turn);
  return {x, terrain.Height(x, z) + above, z};
}

// A gem: five sides round its middle, coming to a point at the top and at
// the bottom, a unit tall.
Mesh Gem() {
  Triangles gem;
  constexpr int kSides = 5;
  const Vector3 top{0.0f, 0.5f, 0.0f};
  const Vector3 bottom{0.0f, -0.5f, 0.0f};
  for (int i = 0; i < kSides; ++i) {
    auto corner = [](int j) {
      const float turn = 2.0f * PI * static_cast<float>(j) / kSides;
      return Vector3{0.5f * std::sin(turn), 0.0f, 0.5f * std::cos(turn)};
    };
    gem.Add(corner(i), corner(i + 1), top, WHITE);
    gem.Add(corner(i + 1), corner(i), bottom, WHITE);
  }
  return gem.Upload();
}

}  // namespace

Fx::Fx(const SkillTable& skills, Shader lit)
    : skills_(skills.by_id),
      light_(LoadShaderFromMemory(kLightVertexShader, kLightFragmentShader)),
      shard_(LoadModelFromMesh(Gem())) {
  shard_.materials[0].shader = lit;
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

Fx::~Fx() {
  UnloadModel(shard_);
  UnloadShader(light_);
}

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
    auto [found, fresh] = casts_.try_emplace(figure.id);
    Cast& cast = found->second;
    // Another skill, or the same one again, is a cast of its own.
    if (fresh || cast.figure.to_skill != figure.to_skill ||
        figure.to_time < cast.figure.to_time) {
      cast.seed = ++seeds_;
      cast.target.reset();
    }
    cast.figure = figure;
    cast.time = figure.to_time;
    // A monster the skill kills is let go of, so the cast keeps where it
    // fell, and the rest of its fx still land there.
    for (const Figure& other : figures) {
      if (other.monster && other.id == figure.target && figure.target != 0) {
        cast.target = other.position;
      }
    }
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
  uint32_t seed;
  uint32_t index;
  Color color;

  float done() const { return age / span; }
  float Random(uint32_t i, uint32_t what) const {
    return nanommo::Random(seed, index, i, what);
  }
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

void DrawRing(const Moment& m, const Terrain& terrain) {
  constexpr int kSides = 48;
  constexpr float kRim = 0.1f;
  const float u = m.done();
  const float radius = std::max(m.fx.size() * EaseOut(u), 0.2f);
  const float width = std::min(0.3f + 0.6f * (1.0f - u), radius);
  const float fade = std::pow(1.0f - u, 1.5f);
  const float wall = 0.4f * (1.0f - u);
  const Color hot = Hot(m.color, 0.6f * (1.0f - u));
  for (int i = 0; i < kSides; ++i) {
    const float turn = 2.0f * PI * static_cast<float>(i) / kSides;
    const float next = 2.0f * PI * static_cast<float>(i + 1) / kSides;
    auto at = [&](float distance, float angle) {
      return Around(terrain, m.at, distance, angle, 0.05f);
    };
    // On the ground: fading in from inside, to a bright rim.
    Quad(at(radius - width, turn), m.color, 0.0f, at(radius - width, next),
         m.color, 0.0f, at(radius - kRim, turn), m.color, 0.35f * fade,
         at(radius - kRim, next), m.color, 0.35f * fade);
    Quad(at(radius - kRim, turn), hot, fade, at(radius - kRim, next), hot, fade,
         at(radius, turn), hot, fade, at(radius, next), hot, fade);
    // And a low wall on the rim, fading upwards.
    Quad(at(radius, turn), hot, 0.7f * fade, at(radius, next), hot, 0.7f * fade,
         Vector3Add(at(radius, turn), Vector3Scale(kUp, wall)), m.color, 0.0f,
         Vector3Add(at(radius, next), Vector3Scale(kUp, wall)), m.color, 0.0f);
  }
}

void DrawSparks(const Moment& m, const Terrain& terrain, Vector3 eye) {
  for (uint32_t i = 0; i < m.fx.count(); ++i) {
    const float life = m.span * (0.45f + 0.55f * m.Random(i, 0));
    const float t = m.age - 0.1f * m.span * m.Random(i, 1);
    if (t < 0.0f || t > life) {
      continue;
    }
    const float rise = (15.0f + 60.0f * m.Random(i, 2)) * DEG2RAD;
    const float turn = 2.0f * PI * m.Random(i, 3);
    const float speed = m.fx.size() / life * (1.0f + 1.5f * m.Random(i, 4));
    const Vector3 way{std::cos(rise) * std::sin(turn), std::sin(rise),
                      std::cos(rise) * std::cos(turn)};
    auto where = [&](float at) {
      return Vector3Add(m.at,
                        Vector3{way.x * speed * at,
                                way.y * speed * at - 0.5f * kGravity * at * at,
                                way.z * speed * at});
    };
    const Vector3 head = where(t);
    if (head.y < terrain.Height(head.x, head.z)) {
      continue;
    }
    const float heat = 1.0f - t / life;
    Streak(where(std::max(t - 0.05f, 0.0f)), head, 0.03f + 0.04f * heat,
           Hot(m.color, heat), heat, eye);
  }
}

void DrawMotes(const Moment& m, const Camera3D& camera) {
  for (uint32_t i = 0; i < m.fx.count(); ++i) {
    const float life = 0.5f * m.span;
    const float t = m.age - 0.5f * m.span * m.Random(i, 0);
    if (t < 0.0f || t > life) {
      continue;
    }
    const float spin =
        (m.Random(i, 1) < 0.5f ? -1.0f : 1.0f) * (2.0f + 2.0f * m.Random(i, 2));
    const float turn = 2.0f * PI * m.Random(i, 3) + spin * t;
    const float out = (0.3f + 0.6f * m.Random(i, 4)) * (1.0f - 0.4f * t / life);
    const Vector3 at = Vector3Add(
        m.at, Vector3{out * std::sin(turn), 0.1f + m.fx.size() * t / life,
                      out * std::cos(turn)});
    Blob(at, 0.06f + 0.06f * m.Random(i, 5), m.color, std::sin(PI * t / life),
         camera);
  }
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

void Fx::DrawSolid(const Terrain& terrain) const {
  Model piece = shard_;
  auto draw = [&](Vector3 at, Vector3 axis, float angle, float turn,
                  Vector3 size, Color color) {
    piece.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = color;
    const Matrix transform = MatrixMultiply(
        MatrixMultiply(MatrixMultiply(MatrixScale(size.x, size.y, size.z),
                                      MatrixRotateY(turn)),
                       MatrixRotate(axis, angle)),
        MatrixTranslate(at.x, at.y, at.z));
    DrawMesh(piece.meshes[0], piece.materials[0], transform);
  };
  for (const auto& [id, cast] : casts_) {
    const skills::Skill& skill = skills_.at(cast.figure.to_skill);
    for (int index = 0; index < skill.look().fx_size(); ++index) {
      const skills::Fx& fx = skill.look().fx(index);
      const float age = cast.time - static_cast<float>(fx.from_ms()) / 1000.0f;
      const float span = static_cast<float>(fx.for_ms()) / 1000.0f;
      const bool there = fx.at() != skills::Fx::TARGET || cast.target;
      if (age < 0.0f || age > span || !there ||
          (fx.kind() != skills::Fx::SHARDS &&
           fx.kind() != skills::Fx::SPIKES)) {
        continue;
      }
      const Vector3 at =
          fx.at() == skills::Fx::TARGET ? *cast.target : cast.figure.position;
      const Moment m{.fx = fx,
                     .at = {at.x, terrain.Height(at.x, at.z), at.z},
                     .age = age,
                     .span = span,
                     .seed = cast.seed,
                     .index = static_cast<uint32_t>(index),
                     .color = GetColor(fx.color())};
      for (uint32_t i = 0; i < fx.count(); ++i) {
        const float turn = 2.0f * PI * m.Random(i, 0);
        if (fx.kind() == skills::Fx::SHARDS) {
          // Thrown up and out, until they land, and shrink away at the end.
          const float rise = (45.0f + 35.0f * m.Random(i, 1)) * DEG2RAD;
          const float speed =
              std::sqrt(fx.size() * kGravity) * (0.6f + 0.6f * m.Random(i, 2));
          const float up = speed * std::sin(rise);
          const float landed = 2.0f * up / kGravity;
          const float t = std::min(age, landed);
          const float out = speed * std::cos(rise) * t;
          const Vector3 here =
              Vector3Add(m.at, Vector3{out * std::sin(turn),
                                       0.1f + up * t - 0.5f * kGravity * t * t,
                                       out * std::cos(turn)});
          const float size = (0.12f + 0.16f * m.Random(i, 3)) *
                             std::min((span - age) / (0.3f * span), 1.0f);
          const Vector3 axis = Vector3Normalize(
              {m.Random(i, 4) - 0.5f, m.Random(i, 5) - 0.5f, 0.3f});
          draw({here.x, std::max(here.y, terrain.Height(here.x, here.z)),
                here.z},
               axis, (6.0f + 10.0f * m.Random(i, 6)) * t, turn,
               {size, 1.4f * size, size}, m.color);
        } else {
          // Out of the ground, nearest first, leaning outwards, and back
          // into it at the end.
          const float distance =
              fx.size() * std::sqrt(0.06f + 0.94f * m.Random(i, 1));
          const float t = age - 0.35f * span * distance / fx.size();
          if (t < 0.0f) {
            continue;
          }
          const float grow = Overshoot(std::min(t / 0.12f, 1.0f)) *
                             std::min((span - age) / (0.25f * span), 1.0f);
          const float height = (0.8f + 1.0f * m.Random(i, 2)) *
                               (1.0f - 0.4f * distance / fx.size());
          const float lean = (15.0f + 20.0f * m.Random(i, 3)) * DEG2RAD;
          const Vector3 axis{std::cos(turn), 0.0f, -std::sin(turn)};
          const Vector3 up = Vector3RotateByAxisAngle(kUp, axis, lean);
          const Vector3 base = Around(terrain, m.at, distance, turn, 0.0f);
          const float width = 0.3f + 0.25f * m.Random(i, 4);
          draw(Vector3Add(base, Vector3Scale(up, height * (grow - 0.7f))), axis,
               lean, 2.0f * PI * m.Random(i, 5), {width, 1.4f * height, width},
               m.color);
        }
      }
    }
  }
}

void Fx::DrawLight(const Camera3D& camera, const Terrain& terrain,
                   const Atmosphere& atmosphere,
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
      if (fx.at() == skills::Fx::TARGET) {
        // Without a monster picked, there's nothing for it to land on.
        if (!cast.target) {
          continue;
        }
        at = *cast.target;
      } else if (fx.at() == skills::Fx::BLADE) {
        Figure posed = cast.figure;
        posed.blend = 1.0f;
        if (const auto blade = characters.Blade(posed)) {
          at = blade->second;
        }
      }
      if (fx.at() != skills::Fx::BLADE) {
        at.y = terrain.Height(at.x, at.z);
      }
      const Moment m{.fx = fx,
                     .at = at,
                     .age = age,
                     .span = span,
                     .seed = cast.seed,
                     .index = static_cast<uint32_t>(index),
                     .color = color};
      rlBegin(RL_TRIANGLES);
      switch (fx.kind()) {
        case skills::Fx::RING:
          DrawRing(m, terrain);
          break;
        case skills::Fx::SPARKS:
          DrawSparks(m, terrain, camera.position);
          break;
        case skills::Fx::MOTES:
          DrawMotes(m, camera);
          break;
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

Vector3 Fx::Shake(Vector3 camera) const {
  Vector3 shake{0.0f, 0.0f, 0.0f};
  for (const auto& [id, cast] : casts_) {
    for (const skills::Fx& fx : skills_.at(cast.figure.to_skill).look().fx()) {
      const float age = cast.time - static_cast<float>(fx.from_ms()) / 1000.0f;
      const float span = static_cast<float>(fx.for_ms()) / 1000.0f;
      if (fx.kind() != skills::Fx::SHAKE || age < 0.0f || age > span) {
        continue;
      }
      // Full within 15 metres of the camera, and nothing past 40.
      const float near = std::clamp(
          1.0f -
              (Vector3Distance(camera, cast.figure.position) - 15.0f) / 25.0f,
          0.0f, 1.0f);
      const float size = fx.size() * std::pow(1.0f - age / span, 2.0f) * near;
      shake =
          Vector3Add(shake, Vector3{size * std::sin(age * 71.0f),
                                    0.6f * size * std::sin(age * 53.0f + 1.3f),
                                    size * std::sin(age * 67.0f + 2.1f)});
    }
  }
  return shake;
}

}  // namespace nanommo
