#include "client/daylight.h"

#include <algorithm>
#include <array>

#include "client/atmosphere.h"
#include "gtest/gtest.h"
#include "protocol/nanommo.pb.h"

namespace {

constexpr int kMeadowsLand = 0;
constexpr int kAshlandsLand = 1;
constexpr int kCryptLand = 2;

nanommo::protocol::Sky At(
    float day,
    nanommo::protocol::Weather weather = nanommo::protocol::WEATHER_CLEAR,
    float strength = 0.0f) {
  nanommo::protocol::Sky sky;
  sky.set_day(day);
  sky.set_weather(weather);
  sky.set_strength(strength);
  return sky;
}

float Brightness(const std::array<float, 3>& c) { return c[0] + c[1] + c[2]; }

}  // namespace

TEST(Daylight, IsDarkAtNightAndLightAllDay) {
  EXPECT_FLOAT_EQ(nanommo::Darkness(0.0f), 1.0f);
  EXPECT_FLOAT_EQ(nanommo::Darkness(0.5f), 0.0f);
  EXPECT_FLOAT_EQ(nanommo::Darkness(0.95f), 1.0f);
  EXPECT_GT(nanommo::Darkness(0.25f), 0.0f);
  EXPECT_LT(nanommo::Darkness(0.25f), 1.0f);
}

TEST(Daylight, RisesInTheEastAndSetsInTheWest) {
  const std::array<float, 3> rising = nanommo::SunAt(0.25f);
  const std::array<float, 3> noon = nanommo::SunAt(0.5f);
  const std::array<float, 3> setting = nanommo::SunAt(0.75f);
  EXPECT_GT(rising[0], 0.9f);
  EXPECT_NEAR(rising[1], 0.0f, 1e-5f);
  EXPECT_GT(noon[1], 0.9f);
  EXPECT_LT(setting[0], -0.9f);
  EXPECT_NEAR(setting[1], 0.0f, 1e-5f);
  // Down under the land at midnight.
  EXPECT_LT(nanommo::SunAt(0.0f)[1], -0.9f);
}

TEST(Daylight, KeepsTheLandsOwnLookAtNoon) {
  const nanommo::Atmosphere noon =
      nanommo::AtmosphereUnder(kMeadowsLand, At(0.5f), {});
  EXPECT_EQ(noon.sky, nanommo::kMeadows.atmosphere.sky);
  EXPECT_EQ(noon.ambient, nanommo::kMeadows.atmosphere.ambient);
  EXPECT_FLOAT_EQ(noon.stars, 0.0f);
  // The sun's overhead, so all of its light comes through.
  EXPECT_EQ(noon.sunlight, nanommo::kMeadows.atmosphere.sunlight);
}

TEST(Daylight, IsDarkerAtNightWithTheMoonUpAndTheStarsOut) {
  const nanommo::Atmosphere night =
      nanommo::AtmosphereUnder(kMeadowsLand, At(0.0f), {});
  const nanommo::Atmosphere noon =
      nanommo::AtmosphereUnder(kMeadowsLand, At(0.5f), {});
  EXPECT_LT(Brightness(night.sky), Brightness(noon.sky));
  EXPECT_LT(Brightness(night.ambient), Brightness(noon.ambient));
  EXPECT_LT(Brightness(night.sunlight), Brightness(noon.sunlight));
  EXPECT_FLOAT_EQ(night.stars, 1.0f);
  // The light comes down from the moon, not up from under the land.
  EXPECT_GT(night.sun[1], 0.9f);
}

TEST(Daylight, GreysAndDarkensTheSkyInTheRain) {
  const nanommo::Atmosphere dry =
      nanommo::AtmosphereUnder(kMeadowsLand, At(0.5f), {});
  const nanommo::Atmosphere wet = nanommo::AtmosphereUnder(
      kMeadowsLand, At(0.5f, nanommo::protocol::WEATHER_RAIN, 1.0f), {});
  EXPECT_LT(Brightness(wet.sunlight), Brightness(dry.sunlight));
  EXPECT_LT(wet.fog_far, dry.fog_far);
  EXPECT_FLOAT_EQ(wet.cloudiness, 1.0f);
  // Grey: its colours closer together than the blue sky's.
  const auto spread = [](const std::array<float, 3>& c) {
    return std::max({c[0], c[1], c[2]}) - std::min({c[0], c[1], c[2]});
  };
  EXPECT_LT(spread(wet.sky), spread(dry.sky));

  // None of it, when it's gone.
  const nanommo::Atmosphere gone = nanommo::AtmosphereUnder(
      kMeadowsLand, At(0.5f, nanommo::protocol::WEATHER_RAIN, 0.0f), {});
  EXPECT_EQ(gone.sky, dry.sky);
}

TEST(Daylight, LightsTheSmokeRedFromBelowInAFirestorm) {
  const nanommo::Atmosphere night =
      nanommo::AtmosphereUnder(kAshlandsLand, At(0.0f), {});
  const nanommo::Atmosphere fire = nanommo::AtmosphereUnder(
      kAshlandsLand, At(0.0f, nanommo::protocol::WEATHER_FIRE, 1.0f), {});
  // Brighter than the night, whatever the time, and red.
  EXPECT_GT(Brightness(fire.sky), Brightness(night.sky));
  EXPECT_GT(fire.sky[0], 2.0f * fire.sky[1]);
  EXPECT_FLOAT_EQ(fire.cloudiness, 1.0f);
  EXPECT_FLOAT_EQ(fire.stars, 0.0f);
}

TEST(Daylight, ShowsTheLandsOwnLookWithDayAndNightTurnedOff) {
  const nanommo::Atmosphere off =
      nanommo::AtmosphereUnder(kMeadowsLand, At(0.0f), {.day_night = false});
  EXPECT_EQ(off.sky, nanommo::kMeadows.atmosphere.sky);
  EXPECT_EQ(off.sun, nanommo::kMeadows.atmosphere.sun);
  EXPECT_EQ(off.sunlight, nanommo::kMeadows.atmosphere.sunlight);
}

TEST(Daylight, KeepsTheSkyClearWithTheWeatherTurnedOff) {
  const nanommo::Atmosphere clear =
      nanommo::AtmosphereUnder(kMeadowsLand, At(0.5f), {});
  const nanommo::Atmosphere off = nanommo::AtmosphereUnder(
      kMeadowsLand, At(0.5f, nanommo::protocol::WEATHER_STORM, 1.0f),
      {.weather = false});
  EXPECT_EQ(off.sky, clear.sky);
  EXPECT_EQ(off.sunlight, clear.sunlight);
  EXPECT_FLOAT_EQ(off.fog_far, clear.fog_far);
}

TEST(Daylight, KeepsTheCryptsNightAllDay) {
  const nanommo::Atmosphere noon =
      nanommo::AtmosphereUnder(kCryptLand, At(0.5f), {});
  const nanommo::Atmosphere midnight =
      nanommo::AtmosphereUnder(kCryptLand, At(0.0f), {});
  EXPECT_EQ(noon.sky, midnight.sky);
  EXPECT_EQ(noon.sun, nanommo::kCrypt.atmosphere.sun);
  EXPECT_GT(noon.stars, 0.0f);
}
