#ifndef NANOMMO_CLIENT_ATMOSPHERE_H_
#define NANOMMO_CLIENT_ATMOSPHERE_H_

#include <raylib.h>

#include <array>

#include "terrain/lands.h"

namespace nanommo {

/// The sky and the light, which the shaders read each frame.
struct Atmosphere {
  /// The background, which the fog fades into.
  std::array<float, 3> sky;
  /// In metres from the camera: the fog starts at the first, and hides
  /// everything past the second.
  float fog_near;
  float fog_far;
  /// Towards the sun.
  std::array<float, 3> sun;
  /// What the sun adds to a face that's turned to it.
  std::array<float, 3> sunlight;
  /// What a face gets even when it's turned away from the sun.
  std::array<float, 3> ambient;
};

/// A land's colours: its atmosphere, and the colours that go into its meshes.
struct Look {
  Atmosphere atmosphere;
  Color sand;
  Color grass;
  Color rock;
  Color snow;
  Color sea;
  /// Multiplies the colours of the trees' faces.
  Color trees;
  /// The light the sea gives off of its own, added to the sun's, where 1 is as
  /// bright as a face turned to a white sun.
  float sea_emission = 0.0f;
};

constexpr Look kMeadows = {
    .atmosphere = {.sky = {0.6f, 0.77f, 0.92f},
                   .fog_near = 45.0f,
                   .fog_far = 150.0f,
                   .sun = {0.55f, 0.75f, 0.35f},
                   .sunlight = {0.75f, 0.68f, 0.55f},
                   .ambient = {0.4f, 0.44f, 0.52f}},
    .sand = {214, 196, 140, 255},
    .grass = {106, 162, 70, 255},
    .rock = {128, 120, 112, 255},
    .snow = {238, 240, 245, 255},
    .sea = {48, 118, 178, 210},
    .trees = WHITE,
};

constexpr Look kAshlands = {
    .atmosphere = {.sky = {0.46f, 0.22f, 0.13f},
                   .fog_near = 35.0f,
                   .fog_far = 130.0f,
                   .sun = {0.6f, 0.45f, -0.4f},
                   .sunlight = {1.0f, 0.55f, 0.3f},
                   .ambient = {0.36f, 0.28f, 0.28f}},
    .sand = {60, 54, 52, 255},
    .grass = {124, 112, 104, 255},
    .rock = {84, 74, 72, 255},
    .snow = {196, 84, 40, 255},
    .sea = {255, 130, 30, 255},
    .trees = {58, 46, 42, 255},
    // Lava.
    .sea_emission = 1.0f,
};

constexpr Look kCrypt = {
    .atmosphere = {.sky = {0.13f, 0.11f, 0.18f},
                   .fog_near = 20.0f,
                   .fog_far = 90.0f,
                   .sun = {0.3f, 0.9f, 0.2f},
                   .sunlight = {0.45f, 0.42f, 0.6f},
                   .ambient = {0.26f, 0.24f, 0.32f}},
    .sand = {70, 66, 74, 255},
    .grass = {88, 92, 84, 255},
    .rock = {104, 98, 112, 255},
    .snow = {150, 146, 160, 255},
    .sea = {40, 200, 150, 255},
    .trees = {70, 64, 80, 255},
    // A glowing green ooze.
    .sea_emission = 0.6f,
};

/// In the order of the lands.
constexpr std::array<Look, 3> kLooks = {kMeadows, kAshlands, kCrypt};
static_assert(kLooks.size() == kLands.size(), "each land needs its look");

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_ATMOSPHERE_H_
