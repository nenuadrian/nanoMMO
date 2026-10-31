#include "client/death_screen.h"

#include "gtest/gtest.h"

namespace {

constexpr float kFrame = 1.0f / 60.0f;

void Wait(nanommo::DeathScreen& screen, float seconds, bool dead) {
  for (float time = 0.0f; time < seconds; time += kFrame) {
    screen.Update(kFrame, dead);
  }
}

}  // namespace

TEST(DeathScreen, DarkensTheWorldWhileTheyreDead) {
  nanommo::DeathScreen screen;
  Wait(screen, 1.0f, false);
  EXPECT_EQ(screen.dark(), 0.0f);

  Wait(screen, nanommo::DeathScreen::kDarken / 2, true);
  EXPECT_GT(screen.dark(), 0.0f);
  EXPECT_LT(screen.dark(), nanommo::DeathScreen::kDim);
  Wait(screen, nanommo::DeathScreen::kDarken, true);
  EXPECT_FLOAT_EQ(screen.dark(), nanommo::DeathScreen::kDim);
}

TEST(DeathScreen, ComesBackFromAllDarkAsTheyComeBack) {
  nanommo::DeathScreen screen;
  Wait(screen, 2.0f, true);

  // All dark on the frame they're back, which hides their jump to the
  // middle, then the world fades in.
  screen.Update(kFrame, false);
  EXPECT_EQ(screen.dark(), 1.0f);
  Wait(screen, nanommo::DeathScreen::kReturn / 2, false);
  EXPECT_GT(screen.dark(), 0.0f);
  EXPECT_LT(screen.dark(), 1.0f);
  Wait(screen, nanommo::DeathScreen::kReturn, false);
  EXPECT_EQ(screen.dark(), 0.0f);
}
