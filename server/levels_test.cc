#include "server/levels.h"

#include "gtest/gtest.h"

TEST(Levels, EachLevelTakesAHundredMoreThanTheLast) {
  EXPECT_EQ(nanommo::LevelFor(0).number, 1);
  EXPECT_EQ(nanommo::LevelFor(99).number, 1);

  const nanommo::Level level = nanommo::LevelFor(450);
  EXPECT_EQ(level.number, 3);
  EXPECT_EQ(level.experience, 150u);
  EXPECT_EQ(level.next, 300u);
}
