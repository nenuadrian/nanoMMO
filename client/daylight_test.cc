#include "client/daylight.h"

#include <array>

#include "client/atmosphere.h"
#include "gtest/gtest.h"
#include "protocol/nanommo.pb.h"

namespace {

constexpr int kMeadowsLand = 0;
constexpr int kCryptLand = 2;

nanommo::protocol::Sky At(float day) {
  nanommo::protocol::Sky sky;
  sky.set_day(day);
  return sky;
}

float Brightness(const std::array<float, 3>& c) { return c[0] + c[1] + c[2]; }

}  // namespace

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

TEST(Daylight, ShowsTheLandsOwnLookWithDayAndNightTurnedOff) {
  const nanommo::Atmosphere off =
      nanommo::AtmosphereUnder(kMeadowsLand, At(0.0f), {.day_night = false});
  EXPECT_EQ(off.sky, nanommo::kMeadows.atmosphere.sky);
  EXPECT_EQ(off.sun, nanommo::kMeadows.atmosphere.sun);
  EXPECT_EQ(off.sunlight, nanommo::kMeadows.atmosphere.sunlight);
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
