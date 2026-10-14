#include "bot/wander.h"

#include <chrono>
#include <optional>
#include <random>
#include <set>
#include <utility>

#include "gtest/gtest.h"

TEST(Wander, HoldsDirectionsLikeTheKeysWould) {
  std::mt19937 random(1);
  std::set<std::pair<float, float>> seen;
  for (int i = 0; i < 1000; ++i) {
    const nanommo::protocol::Input input = nanommo::RandomDirection(random);
    seen.insert({input.x(), input.z()});
  }

  EXPECT_EQ(seen.size(), 9u);
  for (const auto& [x, z] : seen) {
    EXPECT_TRUE(x == -1.0f || x == 0.0f || x == 1.0f);
    EXPECT_TRUE(z == -1.0f || z == 0.0f || z == 1.0f);
  }
}

TEST(Wander, RunsAboutAThirdOfTheTime) {
  std::mt19937 random(1);
  int runs = 0;
  for (int i = 0; i < 1000; ++i) {
    runs += nanommo::RandomDirection(random).run() ? 1 : 0;
  }

  EXPECT_GT(runs, 280);
  EXPECT_LT(runs, 390);
}

TEST(Wander, HoldsForHalfASecondToThreeSeconds) {
  std::mt19937 random(1);
  for (int i = 0; i < 1000; ++i) {
    const auto hold = nanommo::RandomHold(random);
    EXPECT_GE(hold, std::chrono::milliseconds(500));
    EXPECT_LE(hold, std::chrono::milliseconds(3000));
  }
}

TEST(Wander, TravelsUntilItArrivesOrStopsGettingCloser) {
  nanommo::Trip trip{.to_x = 10.0f};
  const std::optional<nanommo::protocol::Input> input =
      nanommo::Travel(trip, 0.0f, 0.0f);
  ASSERT_TRUE(input.has_value());
  EXPECT_FLOAT_EQ(input->x(), 1.0f);
  EXPECT_FLOAT_EQ(input->z(), 0.0f);
  EXPECT_TRUE(input->run());
  EXPECT_FALSE(nanommo::Travel(trip, 8.0f, 0.0f).has_value());

  nanommo::Trip blocked{.to_x = 10.0f};
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(nanommo::Travel(blocked, 4.0f, 0.0f).has_value());
  }
  EXPECT_FALSE(nanommo::Travel(blocked, 4.0f, 0.0f).has_value());
}
