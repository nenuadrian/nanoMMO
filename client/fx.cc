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

Fx::Fx(const SkillTable& skills, Shader lit, const ItemTable& items)
    : skills_(skills.by_id),
      light_(LoadShaderFromMemory(kLightVertexShader, kLightFragmentShader)),
      shard_(LoadModelFromMesh(Gem())) {
  shard_.materials[0].shader = lit;
  for (const auto& [id, item] : items.by_id) {
    if (item.has_weapon()) {
      weapons_[id] = item.weapon();
    }
  }
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
    cast.clock += seconds;
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
        figure.to_time < cast.clock - seconds) {
      cast = Cast{.seed = ++seeds_};
    }
    cast.figure = figure;
    cast.clock = figure.to_time;
    // A monster the skill kills is let go of, so the cast keeps where it
    // fell, and the rest of its fx still land there.
    for (const Figure& other : figures) {
      if (other.monster && other.id == figure.target && figure.target != 0) {
        cast.target = other.position;
      }
    }
  }
  for (auto& [id, cast] : casts_) {
    const skills::Skill& skill = skills_.at(cast.figure.to_skill);
    // Each blow, as the cast comes to it, lands or doesn't, for good.
    const std::vector<Beat> blows = BlowsOf(skill, cast.figure);
    while (cast.landed.size() < blows.size() &&
           cast.clock >= blows[cast.landed.size()].at) {
      cast.landed.push_back(Lands(cast, skill, figures));
    }
    cast.time = cast.clock - Lag(cast);
    cast.figure.to_time = cast.time;
  }
  std::erase_if(casts_, [&](const auto& entry) {
    const Cast& cast = entry.second;
    return cast.time > ends_.at(cast.figure.to_skill);
  });
}

// Half as long again for each metre a blow pushes.
std::vector<Fx::Beat> Fx::BlowsOf(const skills::Skill& skill,
                                  const Figure& figure) const {
  static constexpr float kHeaviest = 2.5f;
  auto weight = [](float push) {
    return std::min(1.0f + 0.5f * push, kHeaviest);
  };
  const skills::Effect& effect = skill.effect();
  std::vector<Beat> blows;
  for (const skills::Blow& blow : effect.blows()) {
    blows.push_back({.at = static_cast<float>(blow.at_ms()) / 1000.0f,
                     .weight = weight(blow.push())});
  }
  if (blows.empty()) {
    const auto weapon = weapons_.find(figure.weapon);
    // A quick swing of the weapon lands as much sooner as it's quicker.
    const float pace = effect.pace() > 0.0f ? effect.pace() : 1.0f;
    const float hit = effect.weapon() && weapon != weapons_.end()
                          ? pace * static_cast<float>(weapon->second.hit_ms())
                          : static_cast<float>(effect.hit_ms());
    blows.push_back({.at = hit / 1000.0f, .weight = weight(effect.push())});
  }
  return blows;
}

// Each blow that's landed holds it back while it's still, and then less and
// less as it catches up.
float Fx::Lag(const Cast& cast) const {
  const skills::Skill& skill = skills_.at(cast.figure.to_skill);
  const float still = static_cast<float>(skill.look().stop_ms()) / 1000.0f;
  if (still <= 0.0f) {
    return 0.0f;
  }
  const std::vector<Beat> blows = BlowsOf(skill, cast.figure);
  float lag = 0.0f;
  for (std::size_t i = 0; i < cast.landed.size(); ++i) {
    const float since = cast.clock - blows[i].at;
    const float held = still * blows[i].weight;
    if (!cast.landed[i] || since < 0.0f) {
      continue;
    }
    const float most = (1.0f - kStill) * held;
    if (since < held) {
      lag += (1.0f - kStill) * since;
    } else if (since < held * (1.0f + kCatchUp)) {
      lag += most * (1.0f - (since - held) / (held * kCatchUp));
    }
  }
  return lag;
}

bool Fx::Lands(Cast& cast, const skills::Skill& skill,
               const std::vector<Figure>& figures) const {
  // A little further than the server's reach, as we see everyone a moment
  // late.
  constexpr float kSlack = 0.5f;
  const skills::Effect& effect = skill.effect();
  const Figure& player = cast.figure;
  const auto weapon = weapons_.find(player.weapon);
  const float reach =
      (effect.weapon() && weapon != weapons_.end() ? weapon->second.reach()
                                                   : effect.reach()) +
      kSlack;
  if (effect.damage() == 0 && !effect.weapon()) {
    return false;
  }
  bool landed = false;
  for (const Figure& other : figures) {
    if (!other.monster || other.to == protocol::MOTION_DEAD ||
        (!effect.all() && other.id != player.target)) {
      continue;
    }
    const float dx = other.position.x - player.position.x;
    const float dz = other.position.z - player.position.z;
    if (std::hypot(dx, dz) > reach) {
      continue;
    }
    if (effect.arc() > 0.0f) {
      const float off =
          std::abs(std::remainder(std::atan2(dx, dz) - player.yaw, 2.0f * PI));
      if (off > effect.arc() * DEG2RAD / 2.0f) {
        continue;
      }
    }
    cast.struck.insert(other.id);
    landed = true;
  }
  return landed;
}

void Fx::Hold(std::vector<Figure>& figures) const {
  for (Figure& figure : figures) {
    float lag = 0.0f;
    if (figure.monster) {
      for (const auto& [id, cast] : casts_) {
        if (cast.struck.contains(figure.id)) {
          lag = std::max(lag, cast.clock - cast.time);
        }
      }
    } else if (const auto cast = casts_.find(figure.id);
               cast != casts_.end() && figure.to == protocol::MOTION_SKILL) {
      lag = cast->second.clock - cast->second.time;
    }
    // Never back past where its clips started, as one that's just changed
    // is hardly into its own.
    figure.from_time = std::max(figure.from_time - lag, 0.0f);
    figure.to_time = std::max(figure.to_time - lag, 0.0f);
  }
}

namespace {

// One fx of a cast, as far as it's got.
struct Moment {
  const skills::Fx& fx;
  /// Where it is.
  Vector3 at;
  /// Which way the player faces.
  float yaw;
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

void DrawBolt(const Moment& m, Vector3 eye) {
  constexpr int kSteps = 16;
  constexpr float kFlicker = 0.07f;
  // A new path every flicker, as lightning strikes again and again.
  const uint32_t strike = static_cast<uint32_t>(m.age / kFlicker);
  const bool lit = std::fmod(m.age, kFlicker) < 0.65f * kFlicker;
  const float bright = std::sqrt(1.0f - m.done()) * (lit ? 1.0f : 0.35f);
  auto jag = [&](uint32_t i, uint32_t what) {
    return m.Random(strike * 101 + i, what) - 0.5f;
  };
  const Vector3 sky =
      Vector3Add(m.at, Vector3{3.0f * jag(0, 0), 16.0f, 3.0f * jag(0, 1)});
  std::array<Vector3, kSteps + 1> path{};
  for (int i = 0; i <= kSteps; ++i) {
    const float along = static_cast<float>(i) / kSteps;
    const float wander = 1.1f * std::sqrt(std::sin(PI * along));
    path[i] = Vector3Add(
        Vector3Lerp(sky, m.at, along),
        Vector3{wander * jag(i + 1, 2), 0.0f, wander * jag(i + 1, 3)});
  }
  for (int i = 0; i < kSteps; ++i) {
    Streak(path[i], path[i + 1], 0.45f, m.color, 0.7f * bright, eye);
    Streak(path[i], path[i + 1], 0.12f, WHITE, bright, eye);
  }
  // Forks, off the path partway down.
  for (uint32_t fork = 0; fork < 3; ++fork) {
    Vector3 at = path[5 + 3 * fork];
    for (uint32_t i = 0; i < 4; ++i) {
      const Vector3 next =
          Vector3Add(at, Vector3{1.6f * jag(20 + fork * 4 + i, 4), -1.1f,
                                 1.6f * jag(20 + fork * 4 + i, 5)});
      Streak(at, next, 0.22f, m.color, 0.5f * bright, eye);
      Streak(at, next, 0.06f, WHITE, 0.8f * bright, eye);
      at = next;
    }
  }
}

void DrawWave(const Moment& m) {
  constexpr int kSteps = 24;
  constexpr float kHalf = 60.0f * DEG2RAD;
  const float u = m.done();
  const float reach = 0.8f + m.fx.size() * u;
  const float fade = std::min(u / 0.1f, 1.0f) * (1.0f - u * u * u);
  const Vector3 middle = Vector3Add(m.at, Vector3Scale(kUp, kMiddle));
  auto at = [&](float turn, float distance, float up) {
    return Vector3Add(middle, Vector3{distance * std::sin(m.yaw + turn), up,
                                      distance * std::cos(m.yaw + turn)});
  };
  const Color hot = Hot(m.color, 0.7f);
  for (int i = 0; i < kSteps; ++i) {
    const float a = -kHalf + 2.0f * kHalf * static_cast<float>(i) / kSteps;
    const float b = -kHalf + 2.0f * kHalf * static_cast<float>(i + 1) / kSteps;
    // Thickest in the middle, and thin at its horns.
    const float thick_a = 0.55f * std::sqrt(std::cos(a / kHalf * PI / 2.0f));
    const float thick_b = 0.55f * std::sqrt(std::cos(b / kHalf * PI / 2.0f));
    // Flat, from nothing behind to bright at the front.
    Quad(at(a, reach - thick_a, 0.0f), m.color, 0.0f,
         at(b, reach - thick_b, 0.0f), m.color, 0.0f, at(a, reach, 0.0f), hot,
         fade, at(b, reach, 0.0f), hot, fade);
    // And standing, bright along the middle.
    for (const float side : {-1.0f, 1.0f}) {
      Quad(at(a, reach, 0.0f), hot, fade, at(b, reach, 0.0f), hot, fade,
           at(a, reach - 0.3f * thick_a, side * 0.6f * thick_a), m.color, 0.0f,
           at(b, reach - 0.3f * thick_b, side * 0.6f * thick_b), m.color, 0.0f);
    }
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

// The blade lit along its length: a bright core down the middle, flickering
// a little, and a glow at the tip. It comes on fast and goes slowly.
void DrawEdge(const Moment& m, const std::pair<Vector3, Vector3>& blade,
              const Camera3D& camera) {
  constexpr float kOn = 0.06f;
  constexpr float kOff = 0.15f;
  const float left = m.span - m.age;
  const float fade = std::min({m.age / kOn, left / kOff, 1.0f});
  const float flicker = 0.85f + 0.15f * std::sin(m.age * 47.0f);
  const auto& [hilt, tip] = blade;
  const Vector3 guard = Vector3Lerp(hilt, tip, 0.18f);
  const float width = m.fx.size() > 0.0f ? m.fx.size() : 0.08f;
  Streak(guard, tip, 3.0f * width, m.color, 0.35f * fade * flicker,
         camera.position);
  Streak(guard, tip, width, Hot(m.color, 0.8f), fade * flicker,
         camera.position);
  Blob(tip, 2.5f * width, m.color, 0.6f * fade, camera);
}

// Cuts across the air, flat to the camera, so that each reads as the stroke
// that made it: drawn across from one end in a flash, then thinning away.
void DrawSlash(const Moment& m, const Camera3D& camera) {
  constexpr int kSteps = 16;
  constexpr float kDraw = 0.07f;
  // A cut after the first comes a little later, and slants its own way.
  constexpr float kStagger = 0.06f;
  constexpr float kSpread = 50.0f * DEG2RAD;
  const Vector3 forward =
      Vector3Normalize(Vector3Subtract(camera.target, camera.position));
  const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, kUp));
  const Vector3 up = Vector3CrossProduct(right, forward);
  const int count = static_cast<int>(std::max(m.fx.count(), 1u));
  for (int j = 0; j < count; ++j) {
    const float age = m.age - kStagger * static_cast<float>(j);
    const float life = m.span - kStagger * static_cast<float>(count - 1);
    if (age < 0.0f || age > life) {
      continue;
    }
    const float drawn = std::min(age / kDraw, 1.0f);
    const float fade = std::pow(1.0f - age / life, 1.5f);
    const float slant = m.fx.angle() * DEG2RAD +
                        (j == 0 ? 0.0f
                                : (j % 2 == 1 ? -1.0f : 1.0f) * kSpread *
                                      (0.6f + 0.4f * m.Random(j, 0)));
    const Vector3 along = Vector3Add(Vector3Scale(right, std::cos(slant)),
                                     Vector3Scale(up, -std::sin(slant)));
    const Vector3 bow = Vector3CrossProduct(forward, along);
    const Vector3 middle = Vector3Add(
        m.at, Vector3Add(Vector3Scale(right, 0.3f * (m.Random(j, 1) - 0.5f)),
                         Vector3Scale(up, 0.3f * (m.Random(j, 2) - 0.5f))));
    const float length = m.fx.size();
    // Widest in the middle and sharp at both ends, bowed a little, and
    // thinner as it fades.
    const float thick = 0.07f * length * (0.35f + 0.65f * fade);
    auto point = [&](float u, float side) {
      const float width = thick * std::sin(PI * u) * side;
      const float curve = 0.08f * length * std::sin(PI * u);
      return Vector3Add(middle,
                        Vector3Add(Vector3Scale(along, length * (u - 0.5f)),
                                   Vector3Scale(bow, curve + width)));
    };
    // Light over the grass takes on its green, so the glow round it is
    // faint, and the bloom does the rest, round a white-hot middle that's
    // nearly solid.
    const Color hot = Hot(m.color, 0.85f);
    for (int i = 0; i < kSteps; ++i) {
      const float a = drawn * static_cast<float>(i) / kSteps;
      const float b = drawn * static_cast<float>(i + 1) / kSteps;
      Quad(point(a, -2.2f), m.color, 0.0f, point(b, -2.2f), m.color, 0.0f,
           point(a, 0.0f), m.color, 0.22f * fade, point(b, 0.0f), m.color,
           0.22f * fade);
      Quad(point(a, 0.0f), m.color, 0.22f * fade, point(b, 0.0f), m.color,
           0.22f * fade, point(a, 2.2f), m.color, 0.0f, point(b, 2.2f), m.color,
           0.0f);
      Quad(point(a, -0.8f), hot, 0.3f * fade, point(b, -0.8f), hot, 0.3f * fade,
           point(a, 0.0f), hot, fade, point(b, 0.0f), hot, fade);
      Quad(point(a, 0.0f), hot, fade, point(b, 0.0f), hot, fade, point(a, 0.8f),
           hot, 0.3f * fade, point(b, 0.8f), hot, 0.3f * fade);
    }
  }
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
      // Thrown out by a blow, they fly on through its hold.
      const float age = cast.clock - static_cast<float>(fx.from_ms()) / 1000.0f;
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
                     .yaw = cast.figure.yaw,
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
      // What goes with the body is held back with it, while what a blow
      // throws out, such as its sparks, flies through the hold.
      const bool held = fx.kind() == skills::Fx::TRAIL ||
                        fx.kind() == skills::Fx::EDGE ||
                        fx.at() == skills::Fx::BLADE;
      const float age = (held ? cast.time : cast.clock) - from;
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
        at.y = terrain.Height(at.x, at.z) + fx.up();
      }
      const Moment m{.fx = fx,
                     .at = at,
                     .yaw = cast.figure.yaw,
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
        case skills::Fx::BOLT:
          DrawBolt(m, camera.position);
          break;
        case skills::Fx::WAVE:
          DrawWave(m);
          break;
        case skills::Fx::GLOW:
          DrawGlow(m, camera, fx.at() != skills::Fx::BLADE);
          break;
        case skills::Fx::EDGE: {
          Figure posed = cast.figure;
          posed.blend = 1.0f;
          if (const auto blade = characters.Blade(posed)) {
            DrawEdge(m, *blade, camera);
          }
          break;
        }
        case skills::Fx::SLASH:
          DrawSlash(m, camera);
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
      const float age = cast.clock - static_cast<float>(fx.from_ms()) / 1000.0f;
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
