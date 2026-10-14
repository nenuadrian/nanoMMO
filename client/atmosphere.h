#ifndef NANOMMO_CLIENT_ATMOSPHERE_H_
#define NANOMMO_CLIENT_ATMOSPHERE_H_

#include <raylib.h>

#include <array>

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
};

constexpr Look kMeadows = {
    .atmosphere = {.sky = {0.62f, 0.78f, 0.9f},
                   .fog_near = 50.0f,
                   .fog_far = 140.0f,
                   .sun = {0.3f, 1.0f, 0.5f},
                   .sunlight = {0.45f, 0.45f, 0.45f},
                   .ambient = {0.55f, 0.55f, 0.55f}},
    .sand = {214, 196, 140, 255},
    .grass = {98, 160, 72, 255},
    .rock = {128, 120, 112, 255},
    .snow = {238, 240, 245, 255},
    .sea = {48, 118, 178, 210},
    .trees = WHITE,
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_ATMOSPHERE_H_
