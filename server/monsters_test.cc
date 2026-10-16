#include "server/monsters.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <random>

#include "gtest/gtest.h"
#include "server/test_content.h"
#include "terrain/terrain.h"

namespace {

constexpr std::chrono::milliseconds kTick(50);

}

TEST(Monsters, RestsThenWalks) {
  std::mt19937 random(7);
  nanommo::Monster monster =
      nanommo::MonsterOf(nanommo::testing::Skeleton(), 0.0f, 0.0f);
  monster.rest = std::chrono::seconds(1);

  for (int tick = 0; tick < 19; ++tick) {
    nanommo::Wander(monster, nanommo::Terrain(), kTick, random);
  }
  EXPECT_EQ(monster.motion, nanommo::Motion::kIdle);
  nanommo::Wander(monster, nanommo::Terrain(), kTick, random);
  EXPECT_EQ(monster.motion, nanommo::Motion::kWalk);
}

TEST(Monsters, StaysNearHomeAndOnTheBoard) {
  std::mt19937 random(7);
  const nanommo::Terrain board;
  const float corner = board.edge();
  nanommo::Monster monster =
      nanommo::MonsterOf(nanommo::testing::Skeleton(), corner, corner);
  monster.home_x = corner;
  monster.home_z = corner;

  for (int tick = 0; tick < 20 * 600; ++tick) {
    nanommo::Wander(monster, board, kTick, random);
    ASSERT_LE(std::hypot(monster.x - corner, monster.z - corner),
              monster.wander + 0.001f);
    ASSERT_LE(monster.x, corner);
    ASSERT_LE(monster.z, corner);
  }
}

TEST(Monsters, RunsAtPlayersItSeesThenSwings) {
  std::mt19937 random(7);
  nanommo::Monster monster =
      nanommo::MonsterOf(nanommo::testing::Skeleton(), 0.0f, 0.0f);
  const std::map<uint32_t, nanommo::Player> players = {{1, {.x = 2.5f}}};

  const nanommo::Terrain board;
  nanommo::Hunt(monster, players, board, kTick, random);
  EXPECT_EQ(monster.prey, 1u);
  EXPECT_EQ(monster.motion, nanommo::Motion::kRun);

  for (int tick = 0; tick < 10; ++tick) {
    nanommo::Hunt(monster, players, board, kTick, random);
  }
  EXPECT_FLOAT_EQ(monster.x, 2.5f - monster.reach);
  EXPECT_EQ(monster.motion, nanommo::Motion::kAttack);
}

TEST(Monsters, SpotsLivingPlayersInSightAndNearHome) {
  nanommo::Monster monster =
      nanommo::MonsterOf(nanommo::testing::Skeleton(), -3.0f, 0.0f);
  monster.home_x = -3.0f;

  EXPECT_EQ(nanommo::Spot(monster, {{1, {.x = 0.5f}}}), 0u);
  EXPECT_EQ(nanommo::Spot(
                monster, {{1, {.x = -1.0f, .motion = nanommo::Motion::kDead}}}),
            0u);
  monster.x = 0.0f;
  EXPECT_EQ(nanommo::Spot(monster, {{1, {.x = 1.5f}}}), 0u);
  EXPECT_EQ(nanommo::Spot(monster, {{1, {.x = -1.0f}}, {2, {.x = 0.5f}}}), 2u);
}

TEST(Monsters, CampsAboveTheBeachAwayFromTheMiddle) {
  const nanommo::Terrain island(1);

  const auto camps = nanommo::Camps(island, 8);

  EXPECT_EQ(camps.size(), 8u);
  for (const auto& [x, z] : camps) {
    EXPECT_GE(std::hypot(x, z), 30.0f);
    EXPECT_GE(island.Height(x, z), 1.0f);
  }
  EXPECT_EQ(nanommo::Camps(island, 8), camps);
}
