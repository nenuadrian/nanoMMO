#include "client/foley.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

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

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

// Up from nothing to its top at peak, a share of the way through, and back
// down to nothing at 1.
float Swell(float t, float peak) {
  const float s = t < peak ? std::sin(0.5f * kPi * t / peak)
                           : std::cos(0.5f * kPi * (t - peak) / (1.0f - peak));
  return std::max(s, 0.0f) * std::max(s, 0.0f);
}

// Air pushed aside: noise in a band that moves from one frequency towards the
// other as it swells, and back as it fades.
Samples Whoosh(Noise& noise, float seconds, float from, float to, float peak) {
  Samples whoosh(SamplesIn(seconds));
  Filter filter;
  constexpr float kQ = 1.4f;
  for (std::size_t i = 0; i < whoosh.size(); ++i) {
    const float swell = Swell(Seconds(i) / seconds, peak);
    const float hertz = from * std::pow(to / from, swell);
    whoosh[i] = filter.Run(noise.Next(), hertz, kQ).band / kQ * swell;
  }
  return whoosh;
}

// How loud the loudest moment of each kind of fx is: the light's chord stays
// under the rest, and the shake's boom and the bolt are the loudest.
float Level(skills::Fx::Kind kind) {
  switch (kind) {
    case skills::Fx::GLOW:
    case skills::Fx::MOTES:
      return 0.2f;
    case skills::Fx::SHARDS:
      return 0.35f;
    case skills::Fx::SPARKS:
      return 0.45f;
    case skills::Fx::TRAIL:
    case skills::Fx::RING:
    case skills::Fx::SPIKES:
    case skills::Fx::WAVE:
      return 0.5f;
    case skills::Fx::BOLT:
    case skills::Fx::SHAKE:
      return 0.7f;
    case skills::Fx::EDGE:
      return 0.15f;
    case skills::Fx::GHOSTS:
      return 0.4f;
    case skills::Fx::SLASH:
      return 0.55f;
    case skills::Fx::FLASH:
      return 0.6f;
    default:
      return 0.5f;
  }
}

}  // namespace

float Coldness(uint32_t color) {
  const float red = static_cast<float>((color >> 24) & 0xff) / 255.0f;
  const float blue = static_cast<float>((color >> 8) & 0xff) / 255.0f;
  return blue - red;
}

Samples FxSound(const skills::Fx& fx, uint32_t seed) {
  Noise noise(seed);
  const float span = static_cast<float>(fx.for_ms()) / 1000.0f;
  const float size = fx.size() > 0.0f ? fx.size() : 1.0f;
  const float cold = Coldness(fx.color());
  // From 110 hertz for red to 440 for blue.
  const float note = 220.0f * std::exp2(cold);
  const int count = static_cast<int>(fx.count() > 0 ? fx.count() : 12);
  Samples sound;
  switch (fx.kind()) {
    case skills::Fx::TRAIL:
      // The blade cutting the air.
      sound = Whoosh(noise, span + 0.1f, 300.0f, 2000.0f, 0.4f);
      break;
    case skills::Fx::GLOW: {
      // A chord that swells and fades with the light: the note, its fifth and
      // its octave, each played twice a little apart, so that they beat.
      sound.resize(SamplesIn(span + 0.5f));
      std::array<Oscillator, 6> voices;
      constexpr std::array<float, 6> kRatios = {1.0f,   1.004f, 1.5f,
                                                1.503f, 2.0f,   2.005f};
      for (std::size_t i = 0; i < sound.size(); ++i) {
        float chord = 0.0f;
        for (std::size_t v = 0; v < voices.size(); ++v) {
          chord += voices[v].Next(note * kRatios[v]);
        }
        sound[i] = chord * Envelope(Seconds(i), 0.4f * span, 0.4f * span);
      }
      FadeOut(sound);
      break;
    }
    case skills::Fx::RING: {
      // A thump, deeper for a bigger ring, and air rushing out along the
      // ground.
      const float low = std::clamp(80.0f / std::sqrt(size), 35.0f, 90.0f);
      Mix(sound, Tone(0.5f, 2.0f * low, low, 0.004f, 0.18f), 0.0f, 0.9f);
      Samples rush(SamplesIn(span));
      Filter filter;
      for (std::size_t i = 0; i < rush.size(); ++i) {
        const float t = Seconds(i);
        rush[i] =
            filter.Run(noise.Next(), 2500.0f * std::pow(0.12f, t / span), 1.0f)
                .band *
            Envelope(t, 0.05f * span, 0.35f * span);
      }
      FadeOut(rush);
      Mix(sound, rush, 0.0f, 0.5f);
      break;
    }
    case skills::Fx::SPARKS: {
      // Each spark a bright ping, high and short, like metal on metal, most
      // of them as they fly off. Two partials that aren't whole multiples of
      // each other ring like metal, where one alone would whistle.
      for (int i = 0; i < std::min(count, 24); ++i) {
        const float at = 0.6f * span * noise.Uniform() * noise.Uniform();
        const float hertz = noise.Between(2500.0f, 5000.0f);
        const float decay = noise.Between(0.01f, 0.03f);
        const float gain = noise.Between(0.3f, 1.0f);
        Mix(sound, Tone(0.15f, hertz, 0.97f * hertz, 0.0005f, decay), at, gain);
        Mix(sound,
            Tone(0.1f, 2.76f * hertz, 2.7f * hertz, 0.0005f, 0.5f * decay), at,
            0.3f * gain);
      }
      // Where they fly off someone, a blow landed: a low note that drops
      // fast, like a drum.
      if (fx.at() == skills::Fx::TARGET) {
        Mix(sound, Tone(0.3f, 160.0f, 50.0f, 0.001f, 0.07f), 0.0f, 3.0f);
      }
      break;
    }
    case skills::Fx::MOTES: {
      // A note for each speck, from a scale with no notes that clash, two
      // octaves above the colour's, and rising as they do.
      constexpr std::array<float, 6> kScale = {
          1.0f, 9.0f / 8.0f, 5.0f / 4.0f, 3.0f / 2.0f, 5.0f / 3.0f, 2.0f};
      for (int i = 0; i < std::min(count, 32); ++i) {
        const float hertz =
            4.0f * note * kScale[static_cast<int>(noise.Uniform() * 5.99f)];
        const float at = 0.8f * span * std::pow(noise.Uniform(), 0.7f);
        Mix(sound, Tone(0.5f, hertz, hertz * 1.03f, 0.004f, 0.12f), at);
      }
      break;
    }
    case skills::Fx::SHAKE:
      // Felt as much as heard: a boom below the notes, and a rumble.
      Mix(sound, Tone(span + 0.4f, 60.0f, 32.0f, 0.005f, 0.5f * span + 0.1f),
          0.0f, 1.0f);
      Mix(sound,
          Hiss(noise, span + 0.4f, 100.0f, 0.7f, Pass::kLow, 0.01f,
               0.5f * span),
          0.0f, 1.0f);
      break;
    case skills::Fx::SHARDS:
      // A crack as the ground breaks, and the pieces coming back down: ice
      // rings like glass, and earth knocks.
      Mix(sound, Click(noise, 2500.0f), 0.0f, 1.0f);
      for (int i = 0; i < std::min(count, 32); ++i) {
        const float at = span * noise.Between(0.4f, 0.95f);
        if (cold > 0.2f) {
          const float hertz = noise.Between(2500.0f, 6000.0f);
          Mix(sound, Tone(0.2f, hertz, hertz, 0.0005f, 0.035f), at, 0.3f);
        } else {
          const float hertz = noise.Between(400.0f, 900.0f);
          Mix(sound, Tone(0.08f, hertz, 0.7f * hertz, 0.0005f, 0.015f), at,
              0.8f);
        }
      }
      break;
    case skills::Fx::SPIKES: {
      // The ground heaves, with a rumble below the notes, and bursts open
      // spike by spike as the ring goes out: each a thunk, a low note that
      // drops fast, and a short knock above it. Ice rings like glass, too.
      Mix(sound, Tone(span, 55.0f, 40.0f, 0.15f * span, 0.4f * span), 0.0f,
          0.8f);
      Mix(sound,
          Hiss(noise, span, 90.0f, 0.7f, Pass::kLow, 0.15f * span, 0.4f * span),
          0.0f, 1.0f);
      const int spikes = std::min(count, 24);
      for (int i = 0; i < spikes; ++i) {
        const float at = 0.35f * span * static_cast<float>(i) / spikes +
                         noise.Between(0.0f, 0.02f);
        const float thunk = noise.Between(150.0f, 220.0f);
        Mix(sound, Tone(0.15f, thunk, 0.35f * thunk, 0.001f, 0.035f), at, 0.6f);
        const float knock = noise.Between(700.0f, 1200.0f);
        Mix(sound, Tone(0.06f, knock, 0.8f * knock, 0.0005f, 0.008f), at,
            0.25f);
        if (cold > 0.2f) {
          const float hertz = noise.Between(2000.0f, 4000.0f);
          Mix(sound, Tone(0.2f, hertz, hertz, 0.0005f, 0.06f), at, 0.15f);
        }
      }
      break;
    }
    case skills::Fx::BOLT: {
      // A crack, the buzz of the bolt, which jumps from one pitch to another,
      // and thunder rolling after it.
      Mix(sound, Click(noise, 2500.0f), 0.0f, 1.0f);
      Samples buzz(SamplesIn(span));
      Filter high;
      Filter low;
      float phase = 0.0f;
      float hertz = 100.0f;
      float level = 1.0f;
      int jump = 0;
      for (std::size_t i = 0; i < buzz.size(); ++i) {
        if (--jump <= 0) {
          jump = SamplesIn(noise.Between(0.008f, 0.025f));
          hertz = noise.Between(50.0f, 160.0f);
          level = noise.Between(0.3f, 1.0f);
        }
        phase += hertz / kRate;
        phase -= std::floor(phase);
        // A sawtooth, which is rich in overtones, and some noise, between
        // 400 hertz and 5,000.
        const float raw = 2.0f * phase - 1.0f + 0.5f * noise.Next();
        buzz[i] = low.Run(high.Run(raw, 400.0f).high, 5000.0f).low * level *
                  Envelope(Seconds(i), 0.003f, 0.6f * span);
      }
      FadeOut(buzz);
      Mix(sound, buzz, 0.0f, 0.5f);
      Mix(sound, Hiss(noise, 1.6f, 120.0f, 0.7f, Pass::kLow, 0.05f, 0.5f),
          0.03f, 1.6f);
      break;
    }
    case skills::Fx::WAVE: {
      // Fire roaring past, and popping.
      Samples roar(SamplesIn(span + 0.2f));
      Filter filter;
      for (std::size_t i = 0; i < roar.size(); ++i) {
        const float swell = Swell(Seconds(i) / (span + 0.2f), 0.3f);
        roar[i] =
            filter.Run(noise.Next(), 400.0f + 1600.0f * swell).low * swell;
      }
      Mix(sound, roar, 0.0f, 1.2f);
      Mix(sound, Crackle(noise, span, 60.0f, 1500.0f, 3.0f, 0.05f, 0.5f * span),
          0.0f, 0.6f);
      break;
    }
    case skills::Fx::EDGE: {
      // The blade singing while it's lit: a high note, two octaves above the
      // colour's, wavering a little, and a second that isn't a whole multiple
      // of it, so that it rings like metal, from a ping as it lights.
      const float hertz = 4.0f * note;
      Samples sing(SamplesIn(span + 0.2f));
      Oscillator first;
      Oscillator second;
      for (std::size_t i = 0; i < sing.size(); ++i) {
        const float t = Seconds(i);
        const float waver = 1.0f + 0.006f * std::sin(2.0f * kPi * 5.5f * t);
        sing[i] = (first.Next(hertz * waver) +
                   0.35f * second.Next(2.76f * hertz * waver)) *
                  Envelope(t, 0.08f, 0.6f * span);
      }
      FadeOut(sing);
      Mix(sound, sing, 0.0f, 1.0f);
      Mix(sound, Tone(0.6f, 2.0f * hertz, 2.0f * hertz, 0.001f, 0.15f), 0.0f,
          0.6f);
      break;
    }
    case skills::Fx::GHOSTS:
      // Air torn as the player goes past, faster than a swing.
      sound = Whoosh(noise, span + 0.15f, 200.0f, 3500.0f, 0.3f);
      break;
    case skills::Fx::SLASH: {
      // Each cut a hiss of air, high up, a snap and a short ring of metal,
      // a little after the one before, as they're drawn.
      const int cuts = static_cast<int>(std::max(fx.count(), 1u));
      for (int i = 0; i < std::min(cuts, 8); ++i) {
        const float at = 0.06f * static_cast<float>(i);
        Mix(sound,
            Hiss(noise, 0.12f, 6000.0f, 0.8f, Pass::kHigh, 0.002f, 0.04f), at,
            0.9f);
        Mix(sound, Click(noise, 5000.0f), at, 0.8f);
        const float ring = noise.Between(2800.0f, 3600.0f);
        Mix(sound, Tone(0.4f, ring, 0.99f * ring, 0.0005f, 0.09f), at, 0.35f);
        Mix(sound, Tone(0.3f, 2.76f * ring, 2.7f * ring, 0.0005f, 0.05f), at,
            0.12f);
      }
      break;
    }
    case skills::Fx::FLASH:
      // A crack, and a thump below it that drops away.
      Mix(sound, Click(noise, 3000.0f), 0.0f, 1.0f);
      Mix(sound, Tone(0.5f, 140.0f, 38.0f, 0.002f, 0.12f), 0.0f, 1.4f);
      break;
    default:
      break;
  }
  Normalize(sound, Level(fx.kind()));
  return sound;
}

Samples SkillSound(const skills::Look& look, uint32_t seed, bool target) {
  Samples sound;
  for (int i = 0; i < look.fx_size(); ++i) {
    const skills::Fx& fx = look.fx(i);
    if (fx.at() == skills::Fx::TARGET && !target) {
      continue;
    }
    Mix(sound, FxSound(fx, seed * 31u + static_cast<uint32_t>(i)),
        static_cast<float>(fx.from_ms()) / 1000.0f);
  }
  Limit(sound);
  return sound;
}

namespace {

// The partials of a struck metal bar, as a glockenspiel's: their frequencies
// over the note's, how loud each is, and how long each rings. They aren't
// whole multiples, which is why it shimmers where a string doesn't.
struct Partial {
  float ratio;
  float gain;
  float decay;
};
constexpr std::array<Partial, 5> kBell = {{{1.0f, 1.0f, 1.2f},
                                           {2.0f, 0.5f, 0.8f},
                                           {2.76f, 0.35f, 0.6f},
                                           {5.4f, 0.2f, 0.35f},
                                           {8.93f, 0.12f, 0.25f}}};

Samples Bell(float seconds, float hertz) {
  Samples bell;
  for (const Partial& partial : kBell) {
    Mix(bell,
        Tone(seconds, hertz * partial.ratio, hertz * partial.ratio, 0.002f,
             partial.decay),
        0.0f, partial.gain);
  }
  return bell;
}

}  // namespace

Samples Departure(uint32_t seed) {
  Noise noise(seed);
  constexpr float kLength = 1.0f;
  Samples sound(SamplesIn(kLength));
  std::array<Oscillator, 3> voices;
  Filter filter;
  float wobble = 0.0f;
  for (std::size_t i = 0; i < sound.size(); ++i) {
    const float t = Seconds(i) / kLength;
    // Two octaves up, wobbling faster and faster.
    const float hertz = 160.0f * std::exp2(2.0f * t);
    wobble += (4.0f + 14.0f * t) / kRate;
    const float tremolo = 0.7f + 0.3f * std::sin(2.0f * kPi * wobble);
    const float tone = voices[0].Next(hertz) + voices[1].Next(hertz * 1.007f) +
                       0.5f * voices[2].Next(hertz * 1.5f);
    const float air =
        filter.Run(noise.Next(), 300.0f * std::exp2(4.0f * t), 1.5f).band /
        1.5f;
    const float rise = t * t;
    const float fall = t > 0.85f ? std::exp(-(t - 0.85f) / 0.05f) : 1.0f;
    sound[i] = (0.25f * tone * tremolo + 0.6f * air) * rise * fall;
  }
  FadeOut(sound);
  Normalize(sound, 0.55f);
  return sound;
}

Samples Arrival(int land) {
  const LandSound& sound = kLandSounds[land];
  Samples arrival;
  // Struck one after the other, low to high, over the root an octave down.
  for (std::size_t i = 0; i < sound.chord.size(); ++i) {
    Mix(arrival, Bell(3.0f, sound.chord[i]), 0.06f * static_cast<float>(i));
  }
  Mix(arrival,
      Tone(2.0f, sound.chord[0] / 2.0f, sound.chord[0] / 2.0f, 0.15f, 0.6f),
      0.0f);
  Noise noise(static_cast<uint32_t>(land));
  Mix(arrival, Whoosh(noise, 0.7f, 5000.0f, 800.0f, 0.1f), 0.0f, 1.5f);
  Normalize(arrival, 0.55f);
  return arrival;
}

}  // namespace nanommo
