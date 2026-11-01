#include "client/foley.h"

#include <cmath>
#include <cstddef>

#include "client/ground.h"

namespace nanommo {

namespace {

// How loud the loudest moment of a step is, whatever it's on.
constexpr float kStepPeak = 0.6f;

float Seconds(std::size_t i) { return static_cast<float>(i) / kRate; }

enum class Pass { kLow, kBand, kHigh };

// The band at one loudness, whatever its q.
float Take(const Filter::Out& out, Pass pass, float q) {
  switch (pass) {
    case Pass::kLow:
      return out.low;
    case Pass::kBand:
      return out.band / q;
    case Pass::kHigh:
      return out.high;
  }
  return out.low;
}

// Noise through a filter, rising over attack and dying away over decay.
Samples Hiss(Noise& noise, float seconds, float hertz, float q, Pass pass,
             float attack, float decay) {
  Samples hiss(SamplesIn(seconds));
  Filter filter;
  for (std::size_t i = 0; i < hiss.size(); ++i) {
    hiss[i] = Take(filter.Run(noise.Next(), hertz, q), pass, q) *
              Envelope(Seconds(i), attack, decay);
  }
  FadeOut(hiss);
  return hiss;
}

// Clicks at random times, about per_second of them while the envelope's at
// its top and fewer as it dies away, each ringing around hertz: grit, crunch
// and crackle, depending on how many and how high.
Samples Crackle(Noise& noise, float seconds, float per_second, float hertz,
                float q, float attack, float decay) {
  Samples crackle(SamplesIn(seconds));
  Filter filter;
  for (std::size_t i = 0; i < crackle.size(); ++i) {
    const float chance =
        per_second / kRate * Envelope(Seconds(i), attack, decay);
    const float click = noise.Uniform() < chance ? noise.Next() : 0.0f;
    // An impulse through a narrow band rings quietly, so it starts loud.
    crackle[i] = 4.0f * filter.Run(click, hertz, q).band;
  }
  FadeOut(crackle);
  return crackle;
}

// The snap at the very start of a hit: a few milliseconds of noise, high up.
Samples Click(Noise& noise, float hertz) {
  return Hiss(noise, 0.03f, hertz, 0.7f, Pass::kBand, 0.0005f, 0.004f);
}

// A sine that glides from one frequency to the other over its length, by the
// one ratio each moment, as we hear pitch.
Samples Tone(float seconds, float from, float to, float attack, float decay) {
  Samples tone(SamplesIn(seconds));
  Oscillator oscillator;
  for (std::size_t i = 0; i < tone.size(); ++i) {
    const float t = Seconds(i);
    tone[i] = oscillator.Next(from * std::pow(to / from, t / seconds)) *
              Envelope(t, attack, decay);
  }
  FadeOut(tone);
  return tone;
}

// The sound again, quieter and duller, as it comes back off walls.
void Echo(Samples& sound, float after, float gain) {
  Samples echo = sound;
  Filter filter;
  for (float& sample : echo) {
    sample = filter.Run(sample, 1500.0f).low;
  }
  Mix(sound, echo, after, gain);
}

}  // namespace

Ground GroundAt(const Terrain& terrain, int land, float x, float z) {
  const float height = terrain.Height(x, z);
  if (height < Terrain::kSeaLevel) {
    return Ground::kWater;
  }
  // How steep it is, from the height half a metre either way.
  constexpr float kStep = 0.5f;
  const float dx = terrain.Height(x + kStep, z) - terrain.Height(x - kStep, z);
  const float dz = terrain.Height(x, z + kStep) - terrain.Height(x, z - kStep);
  const float up =
      2.0f * kStep / std::sqrt(dx * dx + dz * dz + 4.0f * kStep * kStep);
  const LandSound& sound = kLandSounds[land];
  switch (SurfaceAt(height, up)) {
    case Surface::kSand:
      return sound.sand;
    case Surface::kGrass:
      return sound.grass;
    case Surface::kRock:
      return sound.rock;
    case Surface::kSnow:
      return sound.snow;
  }
  return sound.grass;
}

Samples Step(Ground ground, uint32_t seed) {
  Noise noise(seed);
  // The rest of the foot comes down a moment after the heel.
  const float toe = noise.Between(0.04f, 0.07f);
  Samples step;
  switch (ground) {
    case Ground::kGrass:
      // A soft thud, the blades brushing past, and a few snapping.
      Mix(step,
          Hiss(noise, 0.15f, noise.Between(250.0f, 400.0f), 0.7f, Pass::kLow,
               0.002f, 0.03f),
          0.0f, 1.0f);
      Mix(step,
          Hiss(noise, 0.2f, noise.Between(2000.0f, 3500.0f), 1.0f, Pass::kBand,
               0.015f, 0.05f),
          0.0f, 0.2f);
      Mix(step,
          Crackle(noise, 0.15f, 300.0f, noise.Between(2000.0f, 3000.0f), 1.5f,
                  0.01f, 0.04f),
          0.0f, 0.4f);
      Mix(step,
          Hiss(noise, 0.12f, noise.Between(300.0f, 500.0f), 0.7f, Pass::kLow,
               0.004f, 0.025f),
          toe, 0.6f);
      break;
    case Ground::kSand:
      // Sand gives under the foot, with a hiss of fine grains.
      Mix(step,
          Hiss(noise, 0.15f, noise.Between(150.0f, 250.0f), 0.7f, Pass::kLow,
               0.004f, 0.04f),
          0.0f, 1.0f);
      Mix(step,
          Crackle(noise, 0.3f, 3000.0f, noise.Between(3000.0f, 4500.0f), 0.8f,
                  0.01f, 0.07f),
          0.0f, 0.4f);
      Mix(step,
          Crackle(noise, 0.2f, 2000.0f, noise.Between(3000.0f, 4500.0f), 0.8f,
                  0.005f, 0.04f),
          toe, 0.3f);
      break;
    case Ground::kSnow:
      // Snow crunches, as the flakes are pressed together.
      Mix(step,
          Hiss(noise, 0.15f, noise.Between(120.0f, 180.0f), 0.7f, Pass::kLow,
               0.006f, 0.04f),
          0.0f, 0.8f);
      Mix(step,
          Crackle(noise, 0.35f, 1500.0f, noise.Between(1500.0f, 2200.0f), 1.5f,
                  0.02f, 0.08f),
          0.0f, 0.6f);
      Mix(step,
          Crackle(noise, 0.25f, 1200.0f, noise.Between(1800.0f, 2600.0f), 1.5f,
                  0.01f, 0.05f),
          toe, 0.4f);
      break;
    case Ground::kAsh:
      // Cinders, which break with a dry crack.
      Mix(step,
          Hiss(noise, 0.15f, noise.Between(160.0f, 220.0f), 0.7f, Pass::kLow,
               0.003f, 0.05f),
          0.0f, 1.0f);
      Mix(step,
          Crackle(noise, 0.3f, 600.0f, noise.Between(2400.0f, 3200.0f), 4.0f,
                  0.005f, 0.06f),
          0.0f, 0.5f);
      Mix(step,
          Crackle(noise, 0.2f, 500.0f, noise.Between(2800.0f, 3600.0f), 4.0f,
                  0.005f, 0.04f),
          toe, 0.35f);
      break;
    case Ground::kRock:
      // A hard knock, and grit rolling off.
      Mix(step, Click(noise, 4500.0f), 0.0f, 0.4f);
      Mix(step,
          Hiss(noise, 0.1f, noise.Between(1000.0f, 1400.0f), 3.0f, Pass::kBand,
               0.001f, 0.015f),
          0.0f, 1.0f);
      Mix(step,
          Crackle(noise, 0.12f, 250.0f, noise.Between(2000.0f, 3000.0f), 6.0f,
                  0.005f, 0.04f),
          0.01f, 0.5f);
      Mix(step,
          Hiss(noise, 0.1f, noise.Between(1100.0f, 1500.0f), 3.0f, Pass::kBand,
               0.001f, 0.012f),
          toe, 0.6f);
      break;
    case Ground::kStone:
      // A knock that rings, and comes back off the walls.
      Mix(step, Click(noise, 4000.0f), 0.0f, 0.3f);
      Mix(step,
          Hiss(noise, 0.2f, noise.Between(600.0f, 800.0f), 8.0f, Pass::kBand,
               0.001f, 0.04f),
          0.0f, 1.0f);
      Mix(step,
          Hiss(noise, 0.2f, noise.Between(650.0f, 850.0f), 8.0f, Pass::kBand,
               0.001f, 0.03f),
          toe, 0.6f);
      Echo(step, 0.06f, 0.35f);
      Echo(step, 0.13f, 0.15f);
      break;
    case Ground::kWater:
      // A splash, and bubbles, each a note that rises as it closes.
      Mix(step,
          Hiss(noise, 0.3f, noise.Between(1200.0f, 2000.0f), 0.6f, Pass::kBand,
               0.005f, 0.06f),
          0.0f, 0.7f);
      Mix(step, Hiss(noise, 0.3f, 400.0f, 0.7f, Pass::kLow, 0.02f, 0.08f), 0.0f,
          0.8f);
      for (int bubble = 0, n = static_cast<int>(noise.Between(3.0f, 7.0f));
           bubble < n; ++bubble) {
        const float hertz = noise.Between(500.0f, 1300.0f);
        Mix(step, Tone(0.08f, hertz, hertz * 1.6f, 0.002f, 0.02f),
            noise.Between(0.01f, 0.18f), 0.3f);
      }
      break;
  }
  Normalize(step, kStepPeak);
  return step;
}

}  // namespace nanommo
