#include "server/monsters.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
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

TEST(Monsters, RunsAtPlayersItSeesThenSwings) {
  std::mt19937 random(7);
  nanommo::Monster monster = nanommo::Skeleton(0.0f, 0.0f);
  const std::map<uint32_t, nanommo::Player> players = {{1, {.x = 2.5f}}};

  nanommo::Hunt(monster, players, kTick, random);
  EXPECT_EQ(monster.prey, 1u);
  EXPECT_EQ(monster.motion, nanommo::Motion::kRun);

  for (int tick = 0; tick < 10; ++tick) {
    nanommo::Hunt(monster, players, kTick, random);
  }
  EXPECT_FLOAT_EQ(monster.x, 2.5f - nanommo::kMonsterReach);
  EXPECT_EQ(monster.motion, nanommo::Motion::kAttack);
}

TEST(Monsters, SpotsLivingPlayersInSightAndNearHome) {
  nanommo::Monster monster = nanommo::Skeleton(-3.0f, 0.0f);
  monster.home_x = -3.0f;

  EXPECT_EQ(nanommo::Spot(monster, {{1, {.x = 0.5f}}}), 0u);
  EXPECT_EQ(nanommo::Spot(
                monster, {{1, {.x = -1.0f, .motion = nanommo::Motion::kDead}}}),
            0u);
  monster.x = 0.0f;
  EXPECT_EQ(nanommo::Spot(monster, {{1, {.x = 1.5f}}}), 0u);
  EXPECT_EQ(nanommo::Spot(monster, {{1, {.x = -1.0f}}, {2, {.x = 0.5f}}}), 2u);
}
