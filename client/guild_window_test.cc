#include "client/guild_window.h"

#include "gtest/gtest.h"

TEST(GuildWindow, SaysHowLongAWarHasLeftInMinutesAndSeconds) {
  EXPECT_EQ(nanommo::TimeLeft(1800), "30:00");
  EXPECT_EQ(nanommo::TimeLeft(1745), "29:05");
  EXPECT_EQ(nanommo::TimeLeft(59), "0:59");
  EXPECT_EQ(nanommo::TimeLeft(0), "0:00");
}
