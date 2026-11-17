#include "server/weather.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "terrain/lands.h"

namespace nanommo {

namespace {

// How often each weather comes, out of the weights' sum, for each land.
struct Odds {
  protocol::Weather weather;
  int weight;
};

constexpr std::array<Odds, 5> kMeadows = {{{protocol::WEATHER_CLEAR, 7},
                                           {protocol::WEATHER_CLOUDY, 4},
                                           {protocol::WEATHER_RAIN, 5},
                                           {protocol::WEATHER_STORM, 2},
                                           {protocol::WEATHER_MIST, 2}}};
constexpr std::array<Odds, 3> kAshlands = {{{protocol::WEATHER_CLEAR, 7},
                                            {protocol::WEATHER_ASH, 6},
                                            {protocol::WEATHER_FIRE, 7}}};
constexpr std::array<Odds, 2> kCrypt = {
    {{protocol::WEATHER_CLEAR, 1}, {protocol::WEATHER_MIST, 1}}};

// Out in the open, mist settles in the night and lifts in the morning: it
// only comes in a slot that starts between these two times of day. The
// Crypt's night never ends, so its mist comes whenever it likes.
constexpr float kMistFrom = 0.1f;
constexpr float kMistTo = 0.35f;

// The same numbers from the same seed, wherever they're worked out.
uint64_t Mixed(uint64_t z) {
  z += 0x9E3779B97F4A7C15ull;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

// From 0 to 1, the nth number from the seed.
float Fraction(uint64_t seed, int n) {
  return static_cast<float>(Mixed(seed + static_cast<uint64_t>(n)) >> 40) /
         static_cast<float>(1ull << 24);
}

template <std::size_t N>
protocol::Weather Pick(const std::array<Odds, N>& odds, float roll) {
  int total = 0;
  for (const Odds& o : odds) {
    total += o.weight;
  }
  float left = roll * static_cast<float>(total);
  for (const Odds& o : odds) {
    left -= static_cast<float>(o.weight);
    if (left < 0.0f) {
      return o.weather;
    }
  }
  return odds.back().weather;
}

protocol::Weather PickFor(int land, float roll) {
  switch (land) {
    case 1:
      return Pick(kAshlands, roll);
    case 2:
      return Pick(kCrypt, roll);
    default:
      return Pick(kMeadows, roll);
  }
}

float DayAt(int64_t unix_ms) {
  const int64_t into = ((unix_ms % kDayMs) + kDayMs) % kDayMs;
  return static_cast<float>(into) / static_cast<float>(kDayMs);
}

}  // namespace

protocol::Sky SkyAt(int land, int64_t unix_ms) {
  protocol::Sky sky;
  sky.set_day(DayAt(unix_ms));
  const int64_t slot =
      unix_ms >= 0 ? unix_ms / kSlotMs : (unix_ms - kSlotMs + 1) / kSlotMs;
  const int64_t start = slot * kSlotMs;
  const uint64_t seed =
      Mixed(static_cast<uint64_t>(slot) * 7919u +
            kLands[static_cast<std::size_t>(land) % kLands.size()].seed);
  protocol::Weather weather = PickFor(land, Fraction(seed, 0));
  if (weather == protocol::WEATHER_MIST && !kLands[land].dungeon) {
    const float day = DayAt(start);
    if (day < kMistFrom || day > kMistTo) {
      weather = protocol::WEATHER_CLOUDY;
    }
  }
  const int64_t lasts =
      kShortestSpellMs +
      static_cast<int64_t>(Fraction(seed, 1) *
                           static_cast<float>(kSlotMs - kShortestSpellMs));
  const float peak = 0.55f + 0.45f * Fraction(seed, 2);
  const int64_t into = unix_ms - start;
  // Up from nothing as it comes, down to nothing as it goes.
  const float ramp =
      std::clamp(static_cast<float>(std::min(into, lasts - into)) /
                     static_cast<float>(kRampMs),
                 0.0f, 1.0f);
  if (into < lasts && weather != protocol::WEATHER_CLEAR) {
    sky.set_weather(weather);
    sky.set_strength(peak * ramp);
  }
  // A wind of its own each slot, which a storm whips up.
  const float turn = 6.2831853f * Fraction(seed, 3);
  const float storm = sky.weather() == protocol::WEATHER_STORM ||
                              sky.weather() == protocol::WEATHER_FIRE
                          ? sky.strength()
                          : 0.0f;
  const float speed = 1.0f + 2.0f * Fraction(seed, 4) + 4.0f * storm;
  sky.set_wind_x(speed * std::cos(turn));
  sky.set_wind_z(speed * std::sin(turn));
  return sky;
}

int64_t UnixMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace nanommo
