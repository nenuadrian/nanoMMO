#include "server/weather.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>

#include "gtest/gtest.h"
#include "protocol/nanommo.pb.h"

namespace {

constexpr int kMeadows = 0;
constexpr int kAshlands = 1;
constexpr int kCrypt = 2;
constexpr int64_t kMinute = 60 * 1000;

}  // namespace

TEST(Weather, IsTheSameForTheSameLandAndMoment) {
  const int64_t now = 1'763'300'000'000;
  const nanommo::protocol::Sky a = nanommo::SkyAt(kMeadows, now);
  const nanommo::protocol::Sky b = nanommo::SkyAt(kMeadows, now);
  EXPECT_EQ(a.SerializeAsString(), b.SerializeAsString());
}

TEST(Weather, GoesRoundADayInFortyMinutes) {
  EXPECT_FLOAT_EQ(nanommo::SkyAt(kMeadows, 0).day(), 0.0f);
  EXPECT_FLOAT_EQ(nanommo::SkyAt(kMeadows, nanommo::kDayMs / 4).day(), 0.25f);
  EXPECT_FLOAT_EQ(nanommo::SkyAt(kMeadows, nanommo::kDayMs / 2).day(), 0.5f);
  EXPECT_FLOAT_EQ(nanommo::SkyAt(kMeadows, nanommo::kDayMs).day(), 0.0f);
  EXPECT_EQ(nanommo::kDayMs, 40 * kMinute);
}

TEST(Weather, GivesEachLandOnlyItsOwnWeather) {
  std::set<int> seen[3];
  for (int64_t t = 0; t < 400 * nanommo::kSlotMs; t += nanommo::kSlotMs / 3) {
    for (const int land : {kMeadows, kAshlands, kCrypt}) {
      seen[land].insert(nanommo::SkyAt(land, t).weather());
    }
  }
  using nanommo::protocol::Weather;
  EXPECT_EQ(seen[kMeadows],
            (std::set<int>{Weather::WEATHER_CLEAR, Weather::WEATHER_CLOUDY,
                           Weather::WEATHER_RAIN, Weather::WEATHER_STORM,
                           Weather::WEATHER_MIST}));
  EXPECT_EQ(seen[kAshlands],
            (std::set<int>{Weather::WEATHER_CLEAR, Weather::WEATHER_ASH,
                           Weather::WEATHER_FIRE}));
  EXPECT_EQ(seen[kCrypt],
            (std::set<int>{Weather::WEATHER_CLEAR, Weather::WEATHER_MIST}));
}

TEST(Weather, ComesAndGoesInSpellsOfSixToFifteenMinutes) {
  // Each slot's spell, a minute at a time: it starts as the slot does, from
  // nothing, and once it's over the rest of the slot is clear.
  for (int64_t slot = 0; slot < 200; ++slot) {
    const int64_t start = slot * nanommo::kSlotMs;
    const nanommo::protocol::Sky first = nanommo::SkyAt(kMeadows, start);
    EXPECT_FLOAT_EQ(first.strength(), 0.0f);
    if (first.weather() == nanommo::protocol::WEATHER_CLEAR) {
      continue;
    }
    int64_t lasts = 0;
    for (int64_t t = start; t < start + nanommo::kSlotMs; t += kMinute) {
      const nanommo::protocol::Sky sky = nanommo::SkyAt(kMeadows, t);
      EXPECT_LE(sky.strength(), 1.0f);
      if (sky.weather() != nanommo::protocol::WEATHER_CLEAR) {
        EXPECT_EQ(sky.weather(), first.weather());
        lasts = t - start + kMinute;
      }
    }
    EXPECT_GE(lasts, nanommo::kShortestSpellMs);
    EXPECT_LE(lasts, nanommo::kSlotMs);
    // Fully in, once it's had time to come.
    EXPECT_GE(nanommo::SkyAt(kMeadows, start + nanommo::kRampMs).strength(),
              0.55f);
  }
}

TEST(Weather, BringsMistOnlyInTheMorning) {
  for (int64_t t = 0; t < 800 * nanommo::kSlotMs; t += 5 * kMinute) {
    const nanommo::protocol::Sky sky = nanommo::SkyAt(kMeadows, t);
    if (sky.weather() != nanommo::protocol::WEATHER_MIST) {
      continue;
    }
    // The slot it came in started between night's end and mid morning.
    const int64_t start = t - t % nanommo::kSlotMs;
    const float day = nanommo::SkyAt(kMeadows, start).day();
    EXPECT_GE(day, 0.1f);
    EXPECT_LE(day, 0.35f);
  }
}

TEST(Weather, BlowsHarderInAStorm) {
  float calm = 0.0f;
  float storm = 0.0f;
  for (int64_t t = 0; t < 400 * nanommo::kSlotMs; t += nanommo::kSlotMs) {
    const nanommo::protocol::Sky sky =
        nanommo::SkyAt(kMeadows, t + nanommo::kRampMs);
    const float speed = std::hypot(sky.wind_x(), sky.wind_z());
    if (sky.weather() == nanommo::protocol::WEATHER_STORM) {
      storm = std::max(storm, speed);
    } else if (sky.weather() == nanommo::protocol::WEATHER_CLEAR) {
      calm = std::max(calm, speed);
    }
  }
  EXPECT_LE(calm, 3.0f);
  EXPECT_GT(storm, calm);
}
