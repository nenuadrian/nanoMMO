#include "client/daylight.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "terrain/lands.h"

namespace nanommo {

namespace {

using Rgb = std::array<float, 3>;

// A land's looks through the day, beside its own, which is its day.
struct Times {
  Atmosphere dawn;
  Atmosphere dusk;
  Atmosphere night;
};

constexpr Times kMeadowsTimes = {
    .dawn = {.sky = {0.93f, 0.68f, 0.55f},
             .fog_near = 40.0f,
             .fog_far = 140.0f,
             .sunlight = {0.95f, 0.66f, 0.46f},
             .ambient = {0.36f, 0.35f, 0.44f},
             .zenith = {0.38f, 0.45f, 0.7f},
             .disc = {4.0f, 2.7f, 1.7f},
             .disc_size = 4.0f,
             .clouds = {1.0f, 0.8f, 0.7f},
             .cloudiness = 0.5f},
    .dusk = {.sky = {0.96f, 0.55f, 0.34f},
             .fog_near = 40.0f,
             .fog_far = 140.0f,
             .sunlight = {1.0f, 0.55f, 0.3f},
             .ambient = {0.34f, 0.29f, 0.42f},
             .zenith = {0.27f, 0.25f, 0.52f},
             .disc = {4.0f, 2.0f, 0.85f},
             .disc_size = 4.5f,
             .clouds = {1.0f, 0.64f, 0.5f},
             .cloudiness = 0.5f},
    // A moon in place of the sun, and the stars.
    .night = {.sky = {0.06f, 0.08f, 0.16f},
              .fog_near = 25.0f,
              .fog_far = 115.0f,
              .sunlight = {0.24f, 0.29f, 0.44f},
              .ambient = {0.11f, 0.13f, 0.22f},
              .zenith = {0.01f, 0.016f, 0.05f},
              .disc = {1.8f, 1.9f, 2.3f},
              .disc_size = 2.2f,
              .clouds = {0.17f, 0.19f, 0.27f},
              .cloudiness = 0.45f,
              .stars = 1.0f},
};

constexpr Times kAshlandsTimes = {
    .dawn = {.sky = {0.56f, 0.28f, 0.16f},
             .fog_near = 35.0f,
             .fog_far = 130.0f,
             .sunlight = {1.0f, 0.5f, 0.26f},
             .ambient = {0.32f, 0.24f, 0.24f},
             .zenith = {0.24f, 0.11f, 0.09f},
             .disc = {4.0f, 1.2f, 0.4f},
             .disc_size = 6.0f,
             .clouds = {0.5f, 0.33f, 0.28f},
             .cloudiness = 0.8f},
    .dusk = {.sky = {0.5f, 0.18f, 0.1f},
             .fog_near = 35.0f,
             .fog_far = 130.0f,
             .sunlight = {1.0f, 0.42f, 0.2f},
             .ambient = {0.3f, 0.2f, 0.2f},
             .zenith = {0.18f, 0.06f, 0.06f},
             .disc = {4.0f, 1.0f, 0.3f},
             .disc_size = 6.0f,
             .clouds = {0.45f, 0.22f, 0.16f},
             .cloudiness = 0.8f},
    // Through the smoke, a dim red moon, and a few stars. The lava lights
    // the rest.
    .night = {.sky = {0.13f, 0.045f, 0.035f},
              .fog_near = 25.0f,
              .fog_far = 115.0f,
              .sunlight = {0.32f, 0.15f, 0.1f},
              .ambient = {0.15f, 0.09f, 0.08f},
              .zenith = {0.03f, 0.01f, 0.01f},
              .disc = {1.7f, 1.0f, 0.7f},
              .disc_size = 2.8f,
              .clouds = {0.22f, 0.1f, 0.08f},
              .cloudiness = 0.8f,
              .stars = 0.35f},
};

// The Crypt has no days: its stars are out all the time, when the mist lets
// them.
constexpr float kCryptStars = 0.6f;

Rgb Mix(const Rgb& a, const Rgb& b, float t) {
  return {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t,
          a[2] + (b[2] - a[2]) * t};
}

Rgb Scale(const Rgb& a, float s) { return {a[0] * s, a[1] * s, a[2] * s}; }

float Mix(float a, float b, float t) { return a + (b - a) * t; }

float Smooth(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

Atmosphere Mix(const Atmosphere& a, const Atmosphere& b, float t) {
  Atmosphere m = a;
  m.sky = Mix(a.sky, b.sky, t);
  m.fog_near = Mix(a.fog_near, b.fog_near, t);
  m.fog_far = Mix(a.fog_far, b.fog_far, t);
  m.sunlight = Mix(a.sunlight, b.sunlight, t);
  m.ambient = Mix(a.ambient, b.ambient, t);
  m.zenith = Mix(a.zenith, b.zenith, t);
  m.disc = Mix(a.disc, b.disc, t);
  m.disc_size = Mix(a.disc_size, b.disc_size, t);
  m.clouds = Mix(a.clouds, b.clouds, t);
  m.cloudiness = Mix(a.cloudiness, b.cloudiness, t);
  m.stars = Mix(a.stars, b.stars, t);
  return m;
}

Rgb Normalized(const Rgb& v) {
  const float length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  return {v[0] / length, v[1] / length, v[2] / length};
}

// The look at that time of day: night until dawn, dawn into day, the day
// itself, day into dusk, and dusk into night.
Atmosphere Through(const Atmosphere& day, const Times& times, float d) {
  if (d < 0.18f || d >= 0.82f) {
    return times.night;
  }
  if (d < 0.25f) {
    return Mix(times.night, times.dawn, Smooth(0.18f, 0.25f, d));
  }
  if (d < 0.33f) {
    return Mix(times.dawn, day, Smooth(0.25f, 0.33f, d));
  }
  if (d < 0.67f) {
    return day;
  }
  if (d < 0.75f) {
    return Mix(day, times.dusk, Smooth(0.67f, 0.75f, d));
  }
  return Mix(times.dusk, times.night, Smooth(0.75f, 0.82f, d));
}

}  // namespace

std::array<float, 3> SunAt(float day) {
  const float turn = 6.2831853f * (day - 0.25f);
  return Normalized({std::cos(turn), std::sin(turn), 0.35f});
}

Atmosphere AtmosphereUnder(int land, const protocol::Sky& sky) {
  const std::size_t at = static_cast<std::size_t>(land) % kLooks.size();
  Atmosphere a = kLooks[at].atmosphere;
  const bool turns = !kLands[at].dungeon;
  if (!turns) {
    a.stars = kCryptStars;
  } else {
    const Times& times = at == 1 ? kAshlandsTimes : kMeadowsTimes;
    a = Through(kLooks[at].atmosphere, times, sky.day());
    // The sun while it's up, and the moon, across from it, while it's down.
    // Its light fades out as it reaches the horizon, so that the one giving
    // way to the other is never seen.
    const Rgb sun = SunAt(sky.day());
    const Rgb light =
        sun[1] >= 0.0f ? sun : Normalized({-sun[0], -sun[1], sun[2]});
    a.sun = light;
    const float up = Smooth(0.0f, 0.08f, light[1]);
    a.sunlight = Scale(a.sunlight, up);
    a.disc = Scale(a.disc, Smooth(0.0f, 0.04f, light[1]));
  }
  return a;
}

}  // namespace nanommo
