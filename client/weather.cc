#include "client/weather.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "client/glsl.h"

namespace nanommo {

namespace {

// Each quad's colour is its vertices', and how much brighter than that it
// glows rides in its first texture coordinate, so that what glows past white
// blooms. Far away, it fades into the fog, or, added onto what's behind it,
// to nothing.
constexpr const char* kVertexShader = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
out vec4 color;
out float glow;
out float depth;

void main() {
  color = vertexColor;
  glow = vertexTexCoord.x;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
  depth = gl_Position.w;
}
)";

constexpr const char* kFragmentShader = R"(#version 330
in vec4 color;
in float glow;
in float depth;
uniform vec3 fogColor;
uniform float fogNear;
uniform float fogFar;
uniform float added;
out vec4 pixel;

void main() {
  float fog = clamp((depth - fogNear) / (fogFar - fogNear), 0.0, 1.0);
  vec3 lit = color.rgb * glow;
  if (added > 0.5) {
    pixel = vec4(lit * (1.0 - fog) * color.a, 1.0);
  } else {
    pixel = vec4(mix(lit, fogColor, fog), color.a);
  }
}
)";

// What falls, each in a box around the camera that it fills, from its top
// down to the ground. Each one lands somewhere new each time round, and
// starts again at the top.
struct Fall {
  int count;
  /// In metres across.
  float box;
  /// In metres from the ground up.
  float height;
  /// In metres a second.
  float speed;
  uint32_t salt;
};

constexpr Fall kRain = {
    .count = 3200, .box = 38.0f, .height = 20.0f, .speed = 10.0f, .salt = 11};

// In metres in front of the camera: the middle of what falls.
constexpr float kAhead = 12.0f;

// A drop's splash lasts this long, in seconds.
constexpr float kSplash = 0.3f;

constexpr int kMeadowsLand = 0;

uint32_t Hash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7FEB352Du;
  x ^= x >> 15;
  x *= 0x846CA68Bu;
  x ^= x >> 16;
  return x;
}

uint32_t Hash(uint32_t a, uint32_t b) {
  return Hash(a ^ Hash(b + 0x9E3779B9u));
}

// From 0 to 1.
float Unit(uint32_t h) {
  return static_cast<float>(h >> 8) * (1.0f / 16777216.0f);
}

float Wrap(float v, float size) { return v - size * std::floor(v / size); }

// The world's x or z, moved by whole boxes into the box around the middle.
float InBox(float world, float middle, float box) {
  const float low = middle - box / 2.0f;
  return low + Wrap(world - low, box);
}

// Where one of what falls is now, and where it landed the last time round,
// how long ago.
struct Spot {
  Vector3 at;
  Vector3 landed;
  float since;
  /// Where it lands this time round.
  Vector3 lands;
};

Spot Falling(const Fall& fall, int i, float clock, Vector3 middle, Vector2 wind,
             const Terrain& terrain) {
  const uint32_t h = Hash(static_cast<uint32_t>(i), fall.salt);
  const float period = fall.height / fall.speed;
  const float round = clock / period + Unit(h);
  const float k = std::floor(round);
  const float age = round - k;
  const auto landing = [&](float cycle) {
    const uint32_t c =
        Hash(h, static_cast<uint32_t>(static_cast<int64_t>(cycle)));
    const float x = InBox(Unit(c) * fall.box, middle.x, fall.box);
    const float z = InBox(Unit(Hash(c, 7)) * fall.box, middle.z, fall.box);
    return Vector3{x, std::max(terrain.Height(x, z), Terrain::kSeaLevel), z};
  };
  const Vector3 lands = landing(k);
  // Blown by the wind on its way down.
  const float left = (1.0f - age) * period;
  return {.at = {lands.x - wind.x * left, lands.y + (1.0f - age) * fall.height,
                 lands.z - wind.y * left},
          .landed = landing(k - 1.0f),
          .since = age * period,
          .lands = lands};
}

// A corner of a triangle, in a colour, glowing as bright as that.
void Corner(Vector3 at, Color color, float glow) {
  rlColor4ub(color.r, color.g, color.b, color.a);
  rlTexCoord2f(glow, 0.0f);
  rlVertex3f(at.x, at.y, at.z);
}

// A quad's four corners, in one colour, as two triangles.
void Quad(const std::array<Vector3, 4>& corners, Color color, float glow) {
  for (const int i : {0, 1, 2, 0, 2, 3}) {
    Corner(corners[i], color, glow);
  }
}

// From the tail to the head, as wide as that, turned to face the eye.
void Streak(Vector3 head, Vector3 tail, float width, Vector3 eye,
            Color head_color, Color tail_color, float glow) {
  const Vector3 along = Vector3Subtract(head, tail);
  const Vector3 to_eye = Vector3Subtract(eye, head);
  const Vector3 side = Vector3Scale(
      Vector3Normalize(Vector3CrossProduct(along, to_eye)), width / 2.0f);
  const Vector3 a = Vector3Add(head, side);
  const Vector3 b = Vector3Subtract(head, side);
  const Vector3 c = Vector3Subtract(tail, side);
  const Vector3 d = Vector3Add(tail, side);
  Corner(a, head_color, glow);
  Corner(b, head_color, glow);
  Corner(c, tail_color, glow * 0.6f);
  Corner(a, head_color, glow);
  Corner(c, tail_color, glow * 0.6f);
  Corner(d, tail_color, glow * 0.6f);
}

// From nothing within a metre and a half of the eye to all of it from four
// metres on: what falls past the lens fades away, rather than cross the
// picture as a bar.
float AwayFromTheEye(Vector3 at, Vector3 eye) {
  return std::clamp((Vector3Distance(at, eye) - 1.5f) / 2.5f, 0.0f, 1.0f);
}

Color Faded(Color color, float by) {
  color.a = static_cast<unsigned char>(static_cast<float>(color.a) * by);
  return color;
}

// A ring lying on the ground or the water, from inner to outer.
void Ring(Vector3 at, float inner, float outer, Color color,
          float glow = 1.0f) {
  constexpr int kSides = 7;
  for (int i = 0; i < kSides; ++i) {
    const float a = 2.0f * PI * static_cast<float>(i) / kSides;
    const float b = 2.0f * PI * static_cast<float>(i + 1) / kSides;
    const auto point = [&](float r, float turn) {
      return Vector3{at.x + r * std::cos(turn), at.y + 0.04f,
                     at.z + r * std::sin(turn)};
    };
    Quad({point(inner, a), point(outer, a), point(outer, b), point(inner, b)},
         color, glow);
  }
}

Color Shade(const std::array<float, 3>& rgb, float alpha) {
  const auto c = [](float v) {
    return static_cast<unsigned char>(std::clamp(v, 0.0f, 1.0f) * 255.0f);
  };
  return {c(rgb[0]), c(rgb[1]), c(rgb[2]), c(alpha)};
}

void SetFloat(Shader shader, const char* name, float value) {
  SetShaderValue(shader, GetShaderLocation(shader, name), &value,
                 SHADER_UNIFORM_FLOAT);
}

}  // namespace

Weather::Weather() : shader_(LoadGlsl(kVertexShader, kFragmentShader)) {}

Weather::~Weather() { UnloadShader(shader_); }

void Weather::set_land(const Terrain& terrain, int land) {
  terrain_ = terrain;
  land_ = land;
}

void Weather::Update(float seconds, const protocol::Sky& sky, bool on,
                     const Camera3D& camera) {
  clock_ += seconds;
  sky_ = sky;
  on_ = on;

  // Rain soaks the ground over a minute or so, and the sun dries it slower.
  const bool raining = on && land_ == kMeadowsLand &&
                       (sky.weather() == protocol::WEATHER_RAIN ||
                        sky.weather() == protocol::WEATHER_STORM);
  const float soaked = raining ? sky.strength() : 0.0f;
  if (!wetness_) {
    wetness_ = soaked;
  } else if (soaked > *wetness_) {
    wetness_ = std::min(soaked, *wetness_ + seconds / 45.0f);
  } else {
    wetness_ = std::max(soaked, *wetness_ - seconds / 180.0f);
  }

  if (!on || !terrain_ || land_ != kMeadowsLand ||
      sky.weather() != protocol::WEATHER_STORM || sky.strength() < 0.25f) {
    return;
  }
  next_bolt_ -= seconds;
  if (next_bolt_ > 0.0f) {
    return;
  }
  // Somewhere off in the storm, most often ahead of us, where we'd see it,
  // from the clouds to the ground, in a jagged line with a fork off it.
  const uint32_t h = Hash(seed_, ++bolts_struck_);
  const Vector3 ahead = Vector3Subtract(camera.target, camera.position);
  const float wide = Unit(Hash(h, 3)) < 0.7f ? 0.7f : 2.4f;
  const float turn = std::atan2(ahead.z, ahead.x) + wide * (Unit(h) - 0.5f);
  const float away = 110.0f + 230.0f * Unit(Hash(h, 1));
  const float x = camera.position.x + away * std::cos(turn);
  const float z = camera.position.z + away * std::sin(turn);
  const float ground = std::max(terrain_->Height(x, z), Terrain::kSeaLevel);
  constexpr int kSegments = 12;
  constexpr float kCloud = 115.0f;
  strike_.points.clear();
  strike_.branch.clear();
  for (int i = 0; i <= kSegments; ++i) {
    const float down = static_cast<float>(i) / kSegments;
    const float jitter = i == 0 || i == kSegments ? 0.0f : 9.0f;
    strike_.points.push_back(
        {x + jitter * (Unit(Hash(h, 10 + i)) - 0.5f) * 2.0f,
         kCloud + (ground - kCloud) * down,
         z + jitter * (Unit(Hash(h, 40 + i)) - 0.5f) * 2.0f});
  }
  Vector3 fork = strike_.points[4];
  strike_.branch.push_back(fork);
  for (int i = 1; i <= 5; ++i) {
    fork = {fork.x + 7.0f * (Unit(Hash(h, 70 + i)) - 0.3f), fork.y - 9.0f,
            fork.z + 7.0f * (Unit(Hash(h, 90 + i)) - 0.3f)};
    strike_.branch.push_back(fork);
  }
  strike_.at = clock_;
  strike_.near = 1.0f - 0.5f * (away - 110.0f) / 230.0f;
  // The stronger the storm, the more often.
  next_bolt_ = (5.0f + 11.0f * Unit(Hash(h, 2))) / (0.5f + sky.strength());
}

float Weather::StrikeLight() const {
  const float since = clock_ - strike_.at;
  if (since < 0.0f || since > 0.45f) {
    return 0.0f;
  }
  // It flickers: on, nearly off, on again, then fades.
  if (since < 0.07f) {
    return 1.0f;
  }
  if (since < 0.11f) {
    return 0.25f;
  }
  if (since < 0.17f) {
    return 0.9f;
  }
  return 0.9f * (1.0f - (since - 0.17f) / 0.28f);
}

float Weather::flash() const { return 0.85f * strike_.near * StrikeLight(); }

void Weather::Draw(const Camera3D& camera, const Atmosphere& atmosphere) const {
  if (!terrain_ || !on_) {
    return;
  }
  SetShaderValue(shader_, GetShaderLocation(shader_, "fogColor"),
                 atmosphere.sky.data(), SHADER_UNIFORM_VEC3);
  SetFloat(shader_, "fogNear", atmosphere.fog_near);
  SetFloat(shader_, "fogFar", atmosphere.fog_far);
  rlDisableBackfaceCulling();
  rlDisableDepthMask();
  BeginShaderMode(shader_);
  BeginBlendMode(BLEND_ALPHA);
  // What falls fills a box a little in front of the camera, where we'd see
  // it, however far off it looks.
  const Vector3 ahead = Vector3Subtract(camera.target, camera.position);
  const Vector3 level = Vector3Normalize({ahead.x, 0.0f, ahead.z});
  const Vector3 middle =
      Vector3Add(camera.position, Vector3Scale(level, kAhead));
  SetFloat(shader_, "added", 0.0f);
  rlBegin(RL_TRIANGLES);
  DrawFalling(camera, atmosphere, middle);
  rlEnd();
  EndBlendMode();
  // The lightning's added onto what's behind it, and seen through the fog
  // and the rain, however far off.
  BeginBlendMode(BLEND_ADDITIVE);
  SetFloat(shader_, "added", 1.0f);
  SetFloat(shader_, "fogNear", 1e5f);
  SetFloat(shader_, "fogFar", 2e5f);
  rlBegin(RL_TRIANGLES);
  DrawStrike(camera);
  rlEnd();
  EndBlendMode();
  EndShaderMode();
  rlEnableDepthMask();
  rlEnableBackfaceCulling();
}

void Weather::DrawFalling(const Camera3D& camera, const Atmosphere& atmosphere,
                          Vector3 middle) const {
  const Terrain& terrain = *terrain_;
  const Vector2 wind = {sky_.wind_x(), sky_.wind_z()};
  const float strength = std::clamp(sky_.strength(), 0.0f, 1.0f);
  // Lit as much as the sky and the land around it are.
  std::array<float, 3> lit;
  for (int c = 0; c < 3; ++c) {
    lit[c] = 0.5f * atmosphere.sky[c] +
             0.45f * (atmosphere.ambient[c] + 0.6f * atmosphere.sunlight[c]);
  }
  const auto tinted = [&](std::array<float, 3> tint, float alpha) {
    return Shade({lit[0] * tint[0], lit[1] * tint[1], lit[2] * tint[2]}, alpha);
  };

  if (land_ == kMeadowsLand && (sky_.weather() == protocol::WEATHER_RAIN ||
                                sky_.weather() == protocol::WEATHER_STORM)) {
    const bool storm = sky_.weather() == protocol::WEATHER_STORM;
    const int drops = static_cast<int>(kRain.count * strength);
    const Color drop = tinted({1.0f, 1.05f, 1.18f}, storm ? 0.42f : 0.34f);
    const Vector3 down = Vector3Normalize({wind.x, -kRain.speed, wind.y});
    for (int i = 0; i < drops; ++i) {
      const Spot spot = Falling(kRain, i, clock_, middle, wind, terrain);
      const Vector3 tail = Vector3Subtract(spot.at, Vector3Scale(down, 0.7f));
      const Color seen = Faded(drop, AwayFromTheEye(spot.at, camera.position));
      Streak(spot.at, tail, 0.03f, camera.position, seen,
             {seen.r, seen.g, seen.b, 0}, 1.0f);
      // Where it landed a moment ago, a ring that spreads and fades, wider on
      // the water.
      if (spot.since < kSplash) {
        const float t = spot.since / kSplash;
        const bool water = spot.landed.y <= Terrain::kSeaLevel + 0.001f;
        const float r = (water ? 0.08f : 0.04f) + (water ? 0.32f : 0.16f) * t;
        Ring(spot.landed, r * 0.75f, r,
             tinted({1.1f, 1.15f, 1.25f}, (water ? 0.5f : 0.32f) * (1.0f - t)));
      }
    }
  }
}

void Weather::DrawStrike(const Camera3D& camera) const {
  const float light = StrikeLight();
  if (light <= 0.0f) {
    return;
  }
  const Color bolt = {215, 228, 255, 255};
  for (const std::vector<Vector3>* line : {&strike_.points, &strike_.branch}) {
    const float width = line == &strike_.points ? 1.3f : 0.7f;
    for (std::size_t i = 1; i < line->size(); ++i) {
      Streak((*line)[i - 1], (*line)[i], width, camera.position, bolt, bolt,
             9.0f * light);
    }
  }
}

}  // namespace nanommo
