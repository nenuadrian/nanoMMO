#include "client/ambience.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "client/daylight.h"
#include "terrain/lands.h"

namespace nanommo {

namespace {

constexpr float kTau = 6.2831853f;

// Each loop is this long, and fades from its end into its start, so that it
// goes round without a seam.
constexpr float kLoop = 8.0f;
constexpr float kSeam = 0.5f;

// How fast a loop glides to a new level: about two thirds of the way in this
// many seconds.
constexpr float kGlide = 1.5f;

// Sound comes this many metres a second, so thunder comes after the flash.
constexpr float kSoundSpeed = 343.0f;

constexpr int kMeadowsLand = 0;
constexpr int kAshlandsLand = 1;

// Made a little longer than the loop, then folded, so that its last moment
// leads into its first.
Samples Looped(Samples made) {
  const std::size_t length = static_cast<std::size_t>(SamplesIn(kLoop));
  const std::size_t seam = static_cast<std::size_t>(SamplesIn(kSeam));
  made.resize(length + seam, 0.0f);
  for (std::size_t i = 0; i < seam; ++i) {
    const float in = static_cast<float>(i) / static_cast<float>(seam);
    made[i] = made[i] * in + made[length + i] * (1.0f - in);
  }
  made.resize(length);
  return made;
}

// Folds a sound's last moment into its first, so that it goes round without
// a seam, and as long as it was, less that moment.
void Fold(Samples& made, std::size_t seam) {
  if (made.size() <= 2 * seam) {
    return;
  }
  const std::size_t length = made.size() - seam;
  for (std::size_t i = 0; i < seam; ++i) {
    const float in = static_cast<float>(i) / static_cast<float>(seam);
    made[i] = made[i] * in + made[length + i] * (1.0f - in);
  }
  made.resize(length);
}

// The loudness of a sound, as the root of its mean square.
float Rms(const Samples& samples) {
  double sum = 0.0;
  for (const float s : samples) {
    sum += static_cast<double>(s) * s;
  }
  return samples.empty() ? 0.0f
                         : static_cast<float>(std::sqrt(
                               sum / static_cast<double>(samples.size())));
}

// The same sound from a later place in it, which plays beside it as another
// part of the rain.
Samples From(const Samples& samples, std::size_t at) {
  Samples moved(samples.size());
  for (std::size_t i = 0; i < samples.size(); ++i) {
    moved[i] = samples[(at + i) % samples.size()];
  }
  return moved;
}

Samples Silence() {
  return Samples(static_cast<std::size_t>(SamplesIn(kLoop + kSeam)), 0.0f);
}

// A short tone that dies away, added in at a sample: what a drop sounds like
// on a leaf or the ground, or, rising, on water.
void Drop(Samples& into, std::size_t at, float hertz, float rise, float decay,
          float gain) {
  const std::size_t length = static_cast<std::size_t>(SamplesIn(5.0f * decay));
  float phase = 0.0f;
  for (std::size_t i = 0; i < length && at + i < into.size(); ++i) {
    const float t = static_cast<float>(i) / kRate;
    const float f = hertz * (1.0f + rise * std::min(t / decay, 1.0f));
    phase += f / kRate;
    into[at + i] += gain * std::sin(kTau * phase) * std::exp(-t / decay);
  }
}

// Rain: drops, many of them, each a short tick of its own, over a soft low
// bed. Never a hiss, which sounds like a television left on.
Samples Rain(uint32_t seed, float per_second, float loudness, float body) {
  Noise noise(seed);
  Samples s = Silence();
  const int drops = static_cast<int>(per_second * (kLoop + kSeam));
  for (int d = 0; d < drops; ++d) {
    const auto at = static_cast<std::size_t>(noise.Uniform() * s.size());
    const float g = noise.Between(0.2f, 1.0f);
    Drop(s, at, noise.Between(1600.0f, 6800.0f), 0.0f,
         noise.Between(0.0015f, 0.006f), loudness * g * g);
  }
  Filter low;
  for (float& v : s) {
    v += body * low.Run(noise.Next(), 700.0f, 0.5f).low;
  }
  return Looped(std::move(s));
}

// Drops on water ring up in pitch, as each traps a bubble.
Samples OnWater(uint32_t seed) {
  Noise noise(seed);
  Samples s = Silence();
  const int drops = static_cast<int>(140.0f * (kLoop + kSeam));
  for (int d = 0; d < drops; ++d) {
    const auto at = static_cast<std::size_t>(noise.Uniform() * s.size());
    const float g = noise.Between(0.3f, 1.0f);
    Drop(s, at, noise.Between(800.0f, 1900.0f), 0.7f,
         noise.Between(0.008f, 0.016f), 0.16f * g * g);
  }
  return Looped(std::move(s));
}

// The wind: noise through a band that wanders up and down, swelling and
// dying away, which is a whoosh, as slow as a breath.
Samples Wind(uint32_t seed) {
  Noise noise(seed);
  Samples s = Silence();
  Filter band;
  const float a = noise.Between(0.0f, kTau);
  const float b = noise.Between(0.0f, kTau);
  for (std::size_t i = 0; i < s.size(); ++i) {
    const float t = static_cast<float>(i) / kRate;
    // Whole turns in a loop, so that it goes round smoothly.
    const float slow = std::sin(kTau * t / kLoop + a);
    const float slower = std::sin(kTau * 2.0f * t / kLoop + b);
    const float hertz = 420.0f + 180.0f * slow + 90.0f * slower;
    const float swell = 0.55f + 0.3f * slower + 0.15f * slow;
    s[i] = 0.9f * swell * band.Run(noise.Next(), hertz, 1.6f).band;
  }
  return Looped(std::move(s));
}

// Fire falling: crackles in little bursts, a hiss now and then where it lands,
// and a deep rumble under it all.
Samples Fire(uint32_t seed) {
  Noise noise(seed);
  Samples s = Silence();
  Filter high;
  const int bursts = static_cast<int>(9.0f * (kLoop + kSeam));
  for (int b = 0; b < bursts; ++b) {
    auto at = static_cast<std::size_t>(noise.Uniform() * s.size());
    const int clicks = static_cast<int>(noise.Between(2.0f, 9.0f));
    for (int c = 0; c < clicks; ++c) {
      at += static_cast<std::size_t>(SamplesIn(noise.Between(0.004f, 0.04f)));
      const float g = noise.Between(0.2f, 0.7f);
      const auto length =
          static_cast<std::size_t>(SamplesIn(noise.Between(0.0006f, 0.003f)));
      for (std::size_t i = 0; i < length && at + i < s.size(); ++i) {
        s[at + i] += g * high.Run(noise.Next(), 2500.0f).high *
                     (1.0f - static_cast<float>(i) / length);
      }
    }
  }
  const int hisses = static_cast<int>(2.5f * (kLoop + kSeam));
  for (int h = 0; h < hisses; ++h) {
    const auto at = static_cast<std::size_t>(noise.Uniform() * s.size());
    const float hertz = noise.Between(4500.0f, 7000.0f);
    Filter band;
    const auto length = static_cast<std::size_t>(SamplesIn(0.25f));
    for (std::size_t i = 0; i < length && at + i < s.size(); ++i) {
      const float t = static_cast<float>(i) / kRate;
      s[at + i] += 0.12f * Envelope(t, 0.01f, 0.07f) *
                   band.Run(noise.Next(), hertz, 2.0f).band;
    }
  }
  Filter low;
  for (float& v : s) {
    v += 0.5f * low.Run(noise.Next(), 90.0f, 0.6f).low;
  }
  return Looped(std::move(s));
}

// Lava: slow, low bubbles, each a tone that drops as it bursts, over a
// rumble.
Samples Lava(uint32_t seed) {
  Noise noise(seed);
  Samples s = Silence();
  float at = noise.Between(0.0f, 0.5f);
  while (at < kLoop + kSeam) {
    const auto start = static_cast<std::size_t>(SamplesIn(at));
    const float from = noise.Between(110.0f, 190.0f);
    float phase = 0.0f;
    const auto length = static_cast<std::size_t>(SamplesIn(0.25f));
    for (std::size_t i = 0; i < length && start + i < s.size(); ++i) {
      const float t = static_cast<float>(i) / kRate;
      phase += from * (1.0f - 0.55f * std::min(t / 0.15f, 1.0f)) / kRate;
      s[start + i] +=
          0.35f * Envelope(t, 0.02f, 0.06f) * std::sin(kTau * phase);
    }
    at += noise.Between(0.3f, 1.3f);
  }
  Filter low;
  for (float& v : s) {
    v += 0.35f * low.Run(noise.Next(), 60.0f, 0.6f).low;
  }
  return Looped(std::move(s));
}

// Crickets: a few, each chirping on its own pitch and in its own time, a
// chirp being three quick pulses of a high tone.
Samples Crickets(uint32_t seed) {
  Noise noise(seed);
  Samples s = Silence();
  for (int c = 0; c < 4; ++c) {
    const float hertz = noise.Between(4000.0f, 5200.0f);
    const float every = noise.Between(0.6f, 1.2f);
    const float gain = noise.Between(0.05f, 0.12f);
    float at = noise.Between(0.0f, every);
    while (at < kLoop + kSeam) {
      for (int p = 0; p < 3; ++p) {
        const auto start = static_cast<std::size_t>(SamplesIn(at + 0.045f * p));
        const auto length = static_cast<std::size_t>(SamplesIn(0.02f));
        for (std::size_t i = 0; i < length && start + i < s.size(); ++i) {
          const float t = static_cast<float>(i) / kRate;
          const float shape = std::sin(3.1415926f * t / 0.02f);
          s[start + i] += gain * shape * std::sin(kTau * hertz * t);
        }
      }
      at += every * noise.Between(0.9f, 1.1f);
    }
  }
  return Looped(std::move(s));
}

// A bird: a few quick notes, each sliding from one pitch to another, with a
// little trill on it.
Samples Bird(Noise& noise) {
  Samples s;
  float at = 0.0f;
  const int notes = static_cast<int>(noise.Between(2.0f, 6.0f));
  const float low = noise.Between(2200.0f, 3200.0f);
  for (int n = 0; n < notes; ++n) {
    const float from = low * noise.Between(1.0f, 1.7f);
    const float to = low * noise.Between(1.0f, 1.7f);
    const float lasts = noise.Between(0.06f, 0.16f);
    Samples note(static_cast<std::size_t>(SamplesIn(lasts)));
    float phase = 0.0f;
    for (std::size_t i = 0; i < note.size(); ++i) {
      const float t = static_cast<float>(i) / kRate;
      const float f =
          from + (to - from) * t / lasts + 120.0f * std::sin(kTau * 32.0f * t);
      phase += f / kRate;
      note[i] =
          0.25f * std::sin(3.1415926f * t / lasts) * std::sin(kTau * phase);
    }
    Mix(s, note, at);
    at += lasts + noise.Between(0.03f, 0.12f);
  }
  return s;
}

// An owl, far off: hoo, then hoo-hoo, low and soft.
Samples Owl(Noise& noise) {
  Samples s;
  const float hertz = noise.Between(330.0f, 400.0f);
  float at = 0.0f;
  for (const float lasts : {0.42f, 0.22f, 0.36f}) {
    Samples note(static_cast<std::size_t>(SamplesIn(lasts)));
    for (std::size_t i = 0; i < note.size(); ++i) {
      const float t = static_cast<float>(i) / kRate;
      const float shape = std::sin(3.1415926f * t / lasts);
      const float f = hertz * (1.0f - 0.05f * t / lasts);
      note[i] =
          0.3f * shape * shape *
          (std::sin(kTau * f * t) + 0.15f * std::sin(kTau * 2.0f * f * t));
    }
    Mix(s, note, at);
    at += lasts + (at == 0.0f ? 0.45f : 0.08f);
  }
  return s;
}

// A drop of water off the stone, and its echo off the Crypt's walls.
Samples Drip(Noise& noise) {
  Samples plink;
  plink.resize(static_cast<std::size_t>(SamplesIn(0.25f)), 0.0f);
  Drop(plink, 0, noise.Between(1300.0f, 2600.0f), -0.25f, 0.035f, 0.35f);
  Samples s;
  Mix(s, plink, 0.0f);
  Mix(s, plink, 0.17f, 0.4f);
  Mix(s, plink, 0.34f, 0.16f);
  Mix(s, plink, 0.51f, 0.06f);
  return s;
}

float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

}  // namespace

Samples ThunderSound(float metres, uint32_t seed) {
  Noise noise(seed);
  const float lasts = noise.Between(4.0f, 6.5f);
  Samples s(static_cast<std::size_t>(SamplesIn(lasts)), 0.0f);
  // Near, it cracks first, as the air splits.
  if (metres < 220.0f) {
    Filter high;
    const auto length = static_cast<std::size_t>(SamplesIn(0.3f));
    for (std::size_t i = 0; i < length; ++i) {
      const float t = static_cast<float>(i) / kRate;
      s[i] += 0.8f * Envelope(t, 0.002f, 0.06f) *
              high.Run(noise.Next(), 900.0f).high;
    }
  }
  // Then it rolls: low noise, darker as it goes, swelling a few times as
  // the sound comes back off the clouds and the hills.
  Filter low;
  const float a = noise.Between(0.0f, kTau);
  for (std::size_t i = 0; i < s.size(); ++i) {
    const float t = static_cast<float>(i) / kRate;
    const float hertz = 150.0f - 95.0f * std::min(t / lasts, 1.0f);
    const float swells = 0.6f + 0.4f * std::sin(kTau * 0.7f * t + a);
    const float shape = Envelope(t, 0.15f, lasts * 0.35f) * swells;
    s[i] += 2.2f * shape * low.Run(noise.Next(), hertz, 0.8f).low;
  }
  FadeOut(s);
  return s;
}

Hearing HearingOf(int land, const protocol::Sky& sky, bool by_water,
                  bool weather) {
  Hearing h;
  const float strength = weather ? Clamp01(sky.strength()) : 0.0f;
  const protocol::Weather kind =
      weather ? sky.weather() : protocol::WEATHER_CLEAR;
  const float dark = Darkness(sky.day());
  if (kLands[static_cast<std::size_t>(land) % kLands.size()].dungeon) {
    h.drips = 0.9f;
    h.wind = 0.25f + (kind == protocol::WEATHER_MIST ? 0.25f * strength : 0.0f);
    return h;
  }
  if (land == kAshlandsLand) {
    h.lava = 0.55f;
    h.fire = kind == protocol::WEATHER_FIRE ? strength : 0.0f;
    h.wind = 0.2f + (kind == protocol::WEATHER_ASH ? 0.45f * strength : 0.0f) +
             0.4f * h.fire;
    return h;
  }
  if (land == kMeadowsLand) {
    const bool storm = kind == protocol::WEATHER_STORM;
    h.rain = kind == protocol::WEATHER_RAIN || storm ? strength : 0.0f;
    h.downpour = storm ? strength : 0.0f;
    h.water = by_water ? h.rain : 0.0f;
    h.wind = Clamp01(0.12f + 0.25f * h.rain + 0.55f * h.downpour);
    const float dry = Clamp01(1.0f - 1.6f * h.rain);
    h.crickets = dark * dry;
    h.owls = dark * dry;
    // The birds sing most in the morning, and none at night.
    const float morning =
        1.0f - Clamp01(std::abs(sky.day() - 0.32f) / 0.2f) * 0.6f;
    h.birds = (1.0f - dark) * dry * morning;
  }
  return h;
}

Ambience::Ambience(uint32_t seed) : noise_(seed) {
  // Each channel its own, so that it's wide, all round us.
  const uint32_t s = seed * 31u;
  beds_[kRain] = {Rain(s + 1, 420.0f, 0.22f, 0.05f),
                  Rain(s + 2, 420.0f, 0.22f, 0.05f)};
  beds_[kDownpour] = {Rain(s + 3, 2600.0f, 0.12f, 0.16f),
                      Rain(s + 4, 2600.0f, 0.12f, 0.16f)};
  beds_[kWater] = {OnWater(s + 5), OnWater(s + 6)};
  beds_[kWind] = {Wind(s + 7), Wind(s + 8)};
  beds_[kFire] = {Fire(s + 9), Fire(s + 10)};
  beds_[kLava] = {Lava(s + 11), Lava(s + 12)};
  beds_[kCrickets] = {Crickets(s + 13), Crickets(s + 14)};
}

bool Ambience::Recorded(Bed bed, Samples& left, Samples& right) const {
  const std::size_t length = std::min(left.size(), right.size());
  left.resize(length);
  right.resize(length);
  const std::size_t seam = static_cast<std::size_t>(SamplesIn(kSeam));
  Fold(left, seam);
  Fold(right, seam);
  // As loud as the loop it replaces, whatever the recording's own loudness.
  const float made = 0.5f * (Rms(beds_[bed].left) + Rms(beds_[bed].right));
  const float recorded = 0.5f * (Rms(left) + Rms(right));
  if (left.empty() || recorded <= 0.0f) {
    return false;
  }
  const float gain = made / recorded;
  for (Samples* channel : {&left, &right}) {
    for (float& s : *channel) {
      s *= gain;
    }
  }
  return true;
}

void Ambience::UseFire(Samples left, Samples right) {
  if (Recorded(kFire, left, right)) {
    beds_[kFire] = {.left = std::move(left), .right = std::move(right)};
  }
}

void Ambience::UseGust(const Samples& left, const Samples& right) {
  // From the middle, as it blows past, at about the loudness of the wind.
  gust_.resize(std::min(left.size(), right.size()));
  for (std::size_t i = 0; i < gust_.size(); ++i) {
    gust_[i] = 0.5f * (left[i] + right[i]);
  }
  if (const float peak = Peak(gust_); peak > 0.0f) {
    Normalize(gust_, 0.5f);
  }
  FadeOut(gust_);
}

void Ambience::UseRain(Samples left, Samples right) {
  if (!Recorded(kRain, left, right)) {
    return;
  }
  // A downpour is more of it, from other places in the minute, and the rain
  // on the sea is some more again.
  const std::size_t third = left.size() / 3;
  beds_[kDownpour] = {.left = From(left, third), .right = From(right, third)};
  beds_[kWater] = {.left = From(left, 2 * third),
                   .right = From(right, 2 * third)};
  beds_[kRain] = {.left = std::move(left), .right = std::move(right)};
}

void Ambience::Call(const Samples& samples, float gain, float pan, float wait) {
  voices_.push_back({.samples = samples,
                     .wait = static_cast<std::size_t>(SamplesIn(wait)),
                     .gain = gain,
                     .pan = pan});
}

void Ambience::Thunder(float metres, float pan) {
  Call(ThunderSound(metres,
                    static_cast<uint32_t>(noise_.Uniform() * 1e6f) + ++takes_),
       1.0f / (1.0f + metres / 170.0f), pan, metres / kSoundSpeed);
}

void Ambience::Fill(std::span<float> left, std::span<float> right,
                    const Hearing& hearing) {
  const std::array<float, kBeds> wanted = {
      0.55f * hearing.rain,   0.6f * hearing.downpour, 0.45f * hearing.water,
      0.4f * hearing.wind,    0.5f * hearing.fire,     0.45f * hearing.lava,
      0.3f * hearing.crickets};
  const float glide = 1.0f / (kGlide * kRate);
  const std::size_t frames = std::min(left.size(), right.size());
  for (std::size_t i = 0; i < frames; ++i) {
    for (int b = 0; b < kBeds; ++b) {
      Loop& bed = beds_[b];
      bed.level += (wanted[b] - bed.level) * glide;
      if (bed.level > 1e-4f) {
        const std::size_t at = (played_ + i) % bed.left.size();
        left[i] += bed.level * bed.left[at];
        right[i] += bed.level * bed.right[at];
      }
    }
  }
  played_ += frames;

  // Now and then: birds by day, an owl at night, and drips in the Crypt.
  const float seconds = static_cast<float>(frames) / kRate;
  const auto now_and_then = [&](float& next, float level, float every,
                                auto&& make, float gain) {
    if (level < 0.05f) {
      return;
    }
    next -= seconds;
    if (next > 0.0f) {
      return;
    }
    Call(make(noise_), gain * level, noise_.Between(-0.8f, 0.8f));
    next = every * noise_.Between(0.5f, 1.5f) / level;
  };
  now_and_then(next_bird_, hearing.birds, 2.5f, Bird, 0.5f);
  now_and_then(next_owl_, hearing.owls, 28.0f, Owl, 0.45f);
  now_and_then(next_drip_, hearing.drips, 1.6f, Drip, 0.5f);
  // A gust of wind, the windier the more often, when there's one recorded.
  if (!gust_.empty()) {
    now_and_then(
        next_gust_, hearing.wind, 6.0f, [this](Noise&) { return gust_; }, 0.9f);
  }

  for (Voice& voice : voices_) {
    for (std::size_t i = 0; i < frames; ++i) {
      if (voice.wait > 0) {
        --voice.wait;
        continue;
      }
      if (voice.at >= voice.samples.size()) {
        break;
      }
      const float v = voice.gain * voice.samples[voice.at++];
      left[i] += v * std::min(1.0f, 1.0f - voice.pan);
      right[i] += v * std::min(1.0f, 1.0f + voice.pan);
    }
  }
  std::erase_if(voices_,
                [](const Voice& v) { return v.at >= v.samples.size(); });
}

}  // namespace nanommo
