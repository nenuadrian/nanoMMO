#include "client/foley.h"

#include <cstddef>

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

}  // namespace

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
  }
  Normalize(step, kStepPeak);
  return step;
}

}  // namespace nanommo
