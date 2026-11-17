#include "client/ambience.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "client/synth.h"
#include "gtest/gtest.h"
#include "protocol/nanommo.pb.h"

namespace {

constexpr int kMeadowsLand = 0;
constexpr int kAshlandsLand = 1;
constexpr int kCryptLand = 2;

nanommo::protocol::Sky At(float day, nanommo::protocol::Weather weather,
                          float strength) {
  nanommo::protocol::Sky sky;
  sky.set_day(day);
  sky.set_weather(weather);
  sky.set_strength(strength);
  return sky;
}

// So many seconds of it, the left channel and the right one after it.
std::vector<float> Listen(nanommo::Ambience& ambience,
                          const nanommo::Hearing& hearing, float seconds) {
  const auto frames = static_cast<std::size_t>(nanommo::SamplesIn(seconds));
  std::vector<float> left(frames, 0.0f), right(frames, 0.0f);
  ambience.Fill(left, right, hearing);
  left.insert(left.end(), right.begin(), right.end());
  return left;
}

float Loudness(std::span<const float> samples) {
  double sum = 0.0;
  for (const float s : samples) {
    sum += static_cast<double>(s) * s;
  }
  return static_cast<float>(
      std::sqrt(sum / std::max<std::size_t>(samples.size(), 1)));
}

}  // namespace

TEST(Ambience, HearsEachLandsOwnWeather) {
  using nanommo::protocol::WEATHER_FIRE;
  using nanommo::protocol::WEATHER_RAIN;
  const nanommo::Hearing rain = nanommo::HearingOf(
      kMeadowsLand, At(0.5f, WEATHER_RAIN, 0.8f), false, true);
  EXPECT_FLOAT_EQ(rain.rain, 0.8f);
  EXPECT_FLOAT_EQ(rain.water, 0.0f);
  EXPECT_FLOAT_EQ(rain.fire, 0.0f);
  // By the sea, it falls on the water too.
  EXPECT_FLOAT_EQ(
      nanommo::HearingOf(kMeadowsLand, At(0.5f, WEATHER_RAIN, 0.8f), true, true)
          .water,
      0.8f);

  const nanommo::Hearing fire = nanommo::HearingOf(
      kAshlandsLand, At(0.5f, WEATHER_FIRE, 0.6f), false, true);
  EXPECT_FLOAT_EQ(fire.fire, 0.6f);
  EXPECT_FLOAT_EQ(fire.rain, 0.0f);
  EXPECT_GT(fire.lava, 0.0f);

  EXPECT_GT(
      nanommo::HearingOf(kCryptLand, At(0.5f, WEATHER_RAIN, 1.0f), false, true)
          .drips,
      0.0f);

  // Turned off, the rain's quiet.
  EXPECT_FLOAT_EQ(nanommo::HearingOf(kMeadowsLand, At(0.5f, WEATHER_RAIN, 0.8f),
                                     false, false)
                      .rain,
                  0.0f);
}

TEST(Ambience, HasBirdsByDayAndCricketsAndOwlsByNight) {
  using nanommo::protocol::WEATHER_CLEAR;
  const nanommo::Hearing day = nanommo::HearingOf(
      kMeadowsLand, At(0.35f, WEATHER_CLEAR, 0.0f), false, true);
  const nanommo::Hearing night = nanommo::HearingOf(
      kMeadowsLand, At(0.0f, WEATHER_CLEAR, 0.0f), false, true);
  EXPECT_GT(day.birds, 0.5f);
  EXPECT_FLOAT_EQ(day.crickets, 0.0f);
  EXPECT_FLOAT_EQ(night.birds, 0.0f);
  EXPECT_GT(night.crickets, 0.5f);
  EXPECT_GT(night.owls, 0.5f);
  // And none of them sing out in a downpour.
  EXPECT_FLOAT_EQ(
      nanommo::HearingOf(kMeadowsLand,
                         At(0.0f, nanommo::protocol::WEATHER_STORM, 1.0f),
                         false, true)
          .crickets,
      0.0f);
}

TEST(Ambience, IsSilentWithNothingToHear) {
  nanommo::Ambience ambience(7);
  const std::vector<float> heard = Listen(ambience, {}, 1.0f);
  EXPECT_EQ(Loudness(heard), 0.0f);
}

TEST(Ambience, FadesTheRainInRatherThanAllAtOnce) {
  nanommo::Ambience ambience(7);
  const std::vector<float> first = Listen(ambience, {.rain = 1.0f}, 0.1f);
  Listen(ambience, {.rain = 1.0f}, 4.0f);
  const std::vector<float> later = Listen(ambience, {.rain = 1.0f}, 1.0f);
  EXPECT_GT(Loudness(later), 0.01f);
  EXPECT_LT(Loudness(first), 0.25f * Loudness(later));
}

TEST(Ambience, HearsTheThunderAsLongAfterTheFlashAsSoundTakesToCome) {
  nanommo::Ambience ambience(7);
  // 343 metres away is a second away.
  ambience.Thunder(343.0f, 0.0f);
  const std::vector<float> before = Listen(ambience, {}, 0.95f);
  const std::vector<float> after = Listen(ambience, {}, 1.0f);
  EXPECT_EQ(Loudness(before), 0.0f);
  EXPECT_GT(Loudness(after), 0.01f);
}

TEST(Ambience, MakesThunderLouderNearby) {
  const nanommo::Samples near = nanommo::ThunderSound(100.0f, 3);
  const nanommo::Samples far = nanommo::ThunderSound(400.0f, 3);
  // Near, it cracks first: more of it in its first moment.
  const std::size_t moment = static_cast<std::size_t>(nanommo::SamplesIn(0.1f));
  EXPECT_GT(Loudness(std::span(near).first(moment)),
            Loudness(std::span(far).first(moment)));
}

TEST(Ambience, PlaysRecordedRainAsLoudAsTheRainItMakes) {
  nanommo::Ambience made(7);
  Listen(made, {.rain = 1.0f}, 6.0f);
  const float made_loudness = Loudness(Listen(made, {.rain = 1.0f}, 2.0f));

  // A recording ten times louder, and of another length.
  nanommo::Noise noise(3);
  nanommo::Samples left(static_cast<std::size_t>(nanommo::SamplesIn(5.0f)));
  nanommo::Samples right(left.size());
  for (std::size_t i = 0; i < left.size(); ++i) {
    left[i] = 0.5f * noise.Next();
    right[i] = 0.5f * noise.Next();
  }
  nanommo::Ambience recorded(7);
  recorded.UseRain(left, right);
  Listen(recorded, {.rain = 1.0f}, 6.0f);
  const float recorded_loudness =
      Loudness(Listen(recorded, {.rain = 1.0f}, 2.0f));
  EXPECT_NEAR(recorded_loudness, made_loudness, 0.25f * made_loudness);
}

TEST(Ambience, PlaysRecordedFireAsLoudAsTheFireItMakes) {
  nanommo::Ambience made(7);
  Listen(made, {.fire = 1.0f}, 6.0f);
  const float made_loudness = Loudness(Listen(made, {.fire = 1.0f}, 2.0f));

  nanommo::Noise noise(5);
  nanommo::Samples left(static_cast<std::size_t>(nanommo::SamplesIn(5.0f)));
  nanommo::Samples right(left.size());
  for (std::size_t i = 0; i < left.size(); ++i) {
    left[i] = 0.05f * noise.Next();
    right[i] = 0.05f * noise.Next();
  }
  nanommo::Ambience recorded(7);
  recorded.UseFire(left, right);
  Listen(recorded, {.fire = 1.0f}, 6.0f);
  EXPECT_NEAR(Loudness(Listen(recorded, {.fire = 1.0f}, 2.0f)), made_loudness,
              0.25f * made_loudness);
}

TEST(Ambience, BlowsAGustNowAndThenInTheWind) {
  // A gust that's easy to hear: a second of a tone, over a quiet wind.
  nanommo::Samples gust(static_cast<std::size_t>(nanommo::SamplesIn(1.0f)));
  for (std::size_t i = 0; i < gust.size(); ++i) {
    gust[i] = std::sin(0.05f * static_cast<float>(i));
  }
  nanommo::Ambience calm(7);
  calm.UseGust(gust, gust);
  nanommo::Ambience windy(7);
  windy.UseGust(gust, gust);
  // Over a minute, a strong wind has more gusts in it than none at all.
  const std::vector<float> still = Listen(calm, {}, 60.0f);
  const std::vector<float> blowing = Listen(windy, {.wind = 1.0f}, 60.0f);
  EXPECT_EQ(Loudness(still), 0.0f);
  // Louder at its loudest than the same wind without the gusts.
  nanommo::Ambience plain(7);
  const std::vector<float> wind = Listen(plain, {.wind = 1.0f}, 60.0f);
  EXPECT_GT(*std::max_element(blowing.begin(), blowing.end()),
            *std::max_element(wind.begin(), wind.end()) + 0.2f);
}
