#ifndef NANOMMO_TERRAIN_NOISE_H_
#define NANOMMO_TERRAIN_NOISE_H_

#include <cstdint>

namespace nanommo {

/// A number for each point of a grid, which looks random, but is the same
/// for the same seed and point, on any machine.
uint32_t Hash(uint32_t seed, int32_t x, int32_t z);

/// Value noise: from -1 to 1, random at whole numbers, and smooth between them.
float Noise(uint32_t seed, float x, float z);

/// Octaves of noise, each twice as fine and half as strong as the one before.
/// From -1 to 1.
float Fractal(uint32_t seed, float x, float z, int octaves);

}  // namespace nanommo

#endif  // NANOMMO_TERRAIN_NOISE_H_
