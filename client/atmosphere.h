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

/// What a land's sea is made of: its colours, its waves and its light.
struct Liquid {
  /// Out where it's deep. Its alpha is how much of the ground it hides.
  Color deep;
  /// Where it's shallow, near the shore.
  Color shallow;
  /// Along the line where it meets the ground: foam on water, a crust on lava.
  Color edge;
  /// In metres, from the still surface to the top of a wave.
  float height = 0.0f;
  /// In metres, from the top of a wave to the next, of the longest of its
  /// three waves.
  float wavelength = 10.0f;
  /// How fast its waves go, where 1 is as fast as water's.
  float pace = 1.0f;
  /// How much of the sun it reflects to the eye.
  float glint = 0.0f;
  /// The light it gives off of its own, at a wave's middle height, added to
  /// the sun's, where 1 is as bright as a face turned to a white sun. The top
  /// of a wave gives off four times that, and the bottom none.
  float emission = 0.0f;
};

/// A land's colours: its atmosphere, and the colours that go into its meshes.
struct Look {
  Atmosphere atmosphere;
  Color sand;
  Color grass;
  Color rock;
  Color snow;
  Liquid sea;
  /// Multiplies the colours of the trees' faces.
  Color trees;
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
    .sea = {.deep = {48, 118, 178, 210},
            .shallow = {95, 168, 195, 116},
            .edge = {237, 245, 250, 242},
            .height = 0.45f,
            .wavelength = 10.0f,
            .pace = 1.0f,
            .glint = 0.6f},
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
    // Lava, thick and slow, which cools to a crust at the shore.
    .sea = {.deep = {255, 130, 30, 255},
            .shallow = {190, 60, 20, 255},
            .edge = {40, 24, 22, 255},
            .height = 0.3f,
            .wavelength = 12.0f,
            .pace = 0.25f,
            .emission = 0.35f},
    .trees = {58, 46, 42, 255},
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
    // A glowing green ooze.
    .sea = {.deep = {40, 200, 150, 255},
            .shallow = {30, 120, 90, 255},
            .edge = {150, 240, 190, 255},
            .height = 0.3f,
            .wavelength = 8.0f,
            .pace = 0.4f,
            .glint = 0.3f,
            .emission = 0.3f},
    .trees = {70, 64, 80, 255},
};

/// In the order of the lands.
constexpr std::array<Look, 3> kLooks = {kMeadows, kAshlands, kCrypt};
static_assert(kLooks.size() == kLands.size(), "each land needs its look");

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_ATMOSPHERE_H_
