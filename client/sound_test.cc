#include "client/sound.h"

#include "gtest/gtest.h"

TEST(Sound, SlidersFollowTheDecibels) {
  EXPECT_FLOAT_EQ(nanommo::Gain(1.0f), 1.0f);
  EXPECT_FLOAT_EQ(nanommo::Gain(0.5f), 0.1f);
  EXPECT_FLOAT_EQ(nanommo::Gain(0.0f), 0.0f);
}
