#include "client/duel_window.h"

#include "gtest/gtest.h"

TEST(DuelWindow, CountsDownInWholeSecondsRoundedUp) {
  EXPECT_EQ(nanommo::CountdownSeconds(3000), 3u);
  EXPECT_EQ(nanommo::CountdownSeconds(2001), 3u);
  EXPECT_EQ(nanommo::CountdownSeconds(2000), 2u);
  // It says 1 until the duel starts.
  EXPECT_EQ(nanommo::CountdownSeconds(1), 1u);
  EXPECT_EQ(nanommo::CountdownSeconds(0), 0u);
}
