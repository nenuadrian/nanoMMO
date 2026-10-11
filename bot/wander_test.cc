#include "bot/wander.h"

#include <chrono>
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

TEST(Wander, HoldsForHalfASecondToThreeSeconds) {
  std::mt19937 random(1);
  for (int i = 0; i < 1000; ++i) {
    const auto hold = nanommo::RandomHold(random);
    EXPECT_GE(hold, std::chrono::milliseconds(500));
    EXPECT_LE(hold, std::chrono::milliseconds(3000));
  }
}
