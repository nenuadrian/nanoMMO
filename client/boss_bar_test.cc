#include "client/boss_bar.h"

#include <vector>

#include "gtest/gtest.h"

TEST(BossBar, ShowsTheBossYouPickedOrOneNearYou) {
  std::vector<nanommo::Figure> figures = {
      {.id = 1, .you = true},
      {.id = 7, .monster = true, .phase = 1, .position = {40.0f, 0.0f, 0.0f}},
      // Near, and no boss.
      {.id = 8, .monster = true, .position = {2.0f, 0.0f, 0.0f}},
  };
  EXPECT_EQ(nanommo::ShownBoss(figures, 8), nullptr);

  // Picked, it's shown wherever it is, and near you, picked or not.
  ASSERT_NE(nanommo::ShownBoss(figures, 7), nullptr);
  EXPECT_EQ(nanommo::ShownBoss(figures, 7)->id, 7u);
  figures[1].position.x = nanommo::kBossBarReach - 1.0f;
  ASSERT_NE(nanommo::ShownBoss(figures, 0), nullptr);
  EXPECT_EQ(nanommo::ShownBoss(figures, 0)->id, 7u);

  // Not once it's dead.
  figures[1].to = nanommo::protocol::MOTION_DEAD;
  EXPECT_EQ(nanommo::ShownBoss(figures, 7), nullptr);
}
