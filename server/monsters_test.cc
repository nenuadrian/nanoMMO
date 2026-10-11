#include "server/monsters.h"

#include <chrono>
#include <cmath>
#include <random>

#include "gtest/gtest.h"

namespace {

constexpr std::chrono::milliseconds kTick(50);

}

TEST(Monsters, RestsThenWalks) {
  std::mt19937 random(7);
  nanommo::Monster monster = nanommo::Skeleton(0.0f, 0.0f);
  monster.rest = std::chrono::seconds(1);

  for (int tick = 0; tick < 19; ++tick) {
    nanommo::Wander(monster, kTick, random);
  }
  EXPECT_EQ(monster.motion, nanommo::Motion::kIdle);
  nanommo::Wander(monster, kTick, random);
  EXPECT_EQ(monster.motion, nanommo::Motion::kWalk);
}

TEST(Monsters, StaysNearHomeAndOnTheBoard) {
  std::mt19937 random(7);
  const float corner = nanommo::World::kEdge;
  nanommo::Monster monster = nanommo::Skeleton(corner, corner);
  monster.home_x = corner;
  monster.home_z = corner;

  for (int tick = 0; tick < 20 * 600; ++tick) {
    nanommo::Wander(monster, kTick, random);
    ASSERT_LE(std::hypot(monster.x - corner, monster.z - corner),
              nanommo::kWanderRadius + 0.001f);
    ASSERT_LE(monster.x, corner);
    ASSERT_LE(monster.z, corner);
  }
}
