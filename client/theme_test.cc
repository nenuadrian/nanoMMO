#include "client/theme.h"

#include "gtest/gtest.h"

TEST(Theme, ScalesTheInterfaceWithTheScreen) {
  // At 1080 lines it's its own size, twice that at 4K, and the player's own
  // scale is on top.
  EXPECT_FLOAT_EQ(nanommo::InterfaceScale(1080.0f, 1.0f), 1.0f);
  EXPECT_FLOAT_EQ(nanommo::InterfaceScale(2160.0f, 1.0f), 2.0f);
  EXPECT_FLOAT_EQ(nanommo::InterfaceScale(1080.0f, 1.25f), 1.25f);
  // On a small screen, it stops shrinking before it can't be read.
  EXPECT_FLOAT_EQ(nanommo::InterfaceScale(600.0f, 1.0f), 0.75f);
  // And a scale in the settings file past its ends is held to them.
  EXPECT_FLOAT_EQ(nanommo::InterfaceScale(1080.0f, 10.0f), 2.0f);
}

TEST(Theme, FadesAColour) {
  EXPECT_EQ(nanommo::Faded(IM_COL32(10, 20, 30, 200), 0.5f),
            IM_COL32(10, 20, 30, 100));
  EXPECT_EQ(nanommo::Faded(IM_COL32(10, 20, 30, 200), 2.0f),
            IM_COL32(10, 20, 30, 200));
}
