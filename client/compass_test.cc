#include "client/compass.h"

#include <raylib.h>

#include <numbers>
#include <optional>
#include <vector>

#include "client/animator.h"
#include "gtest/gtest.h"

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

}  // namespace

TEST(Compass, BearingsGoClockwiseFromNorthUpTheMap) {
  const Vector3 here{0.0f, 0.0f, 0.0f};
  // North is up the map, along minus z, and east along plus x.
  EXPECT_NEAR(nanommo::Bearing(here, {0.0f, 0.0f, -5.0f}), 0.0f, 1e-5f);
  EXPECT_NEAR(nanommo::Bearing(here, {5.0f, 0.0f, 0.0f}), kPi / 2, 1e-5f);
  EXPECT_NEAR(std::abs(nanommo::Bearing(here, {0.0f, 0.0f, 5.0f})), kPi, 1e-5f);
  EXPECT_NEAR(nanommo::Bearing(here, {-5.0f, 0.0f, 0.0f}), -kPi / 2, 1e-5f);
}

TEST(Compass, HeadsTheWayTheCameraLooks) {
  // From behind the player, south of them, it looks north.
  const Camera3D camera{.position = {0.0f, 5.0f, 10.0f},
                        .target = {0.0f, 1.0f, 0.0f},
                        .up = {0.0f, 1.0f, 0.0f},
                        .fovy = 50.0f,
                        .projection = CAMERA_PERSPECTIVE};
  EXPECT_NEAR(nanommo::Heading(camera), 0.0f, 1e-5f);
}

TEST(Compass, PutsABearingAcrossItsSpan) {
  // Looking east, east is in the middle, north at the left end and south at
  // the right.
  EXPECT_NEAR(*nanommo::CompassAt(kPi / 2, kPi / 2, kPi), 0.0f, 1e-5f);
  EXPECT_NEAR(*nanommo::CompassAt(kPi / 2, 0.0f, kPi), -1.0f, 1e-5f);
  EXPECT_NEAR(*nanommo::CompassAt(kPi / 2, kPi, kPi), 1.0f, 1e-5f);
  // West is behind, so off it.
  EXPECT_FALSE(nanommo::CompassAt(kPi / 2, -kPi / 2, kPi).has_value());
}

TEST(Compass, GoesRoundTheShorterWay) {
  // Looking a little west of north, a little east of north is just to the
  // right, not most of a turn away.
  const std::optional<float> at = nanommo::CompassAt(2 * kPi - 0.1f, 0.1f, kPi);
  ASSERT_TRUE(at.has_value());
  EXPECT_NEAR(*at, 0.2f / (kPi / 2), 1e-4f);
}

TEST(Compass, MarksThePickedMonsterAndTheQuestGivers) {
  nanommo::Figure you{.id = 1, .you = true, .position = {0.0f, 0.0f, 0.0f}};
  nanommo::Figure monster{
      .id = 7, .monster = true, .position = {3.0f, 0, 4.0f}};
  nanommo::Figure other{.id = 8, .monster = true, .position = {9.0f, 0, 9.0f}};
  nanommo::Figure giver{
      .id = 2, .npc = true, .mark = '!', .position = {0.0f, 0.0f, -8.0f}};
  const std::vector<nanommo::CompassMark> marks =
      nanommo::CompassMarks({you, monster, other, giver}, 1, 7, std::nullopt);
  int targets = 0;
  int quests = 0;
  for (const nanommo::CompassMark& mark : marks) {
    if (mark.kind == nanommo::CompassMark::Kind::kTarget) {
      ++targets;
      EXPECT_NEAR(mark.distance, 5.0f, 1e-4f);
    }
    if (mark.kind == nanommo::CompassMark::Kind::kQuest) {
      ++quests;
      EXPECT_NEAR(mark.bearing, 0.0f, 1e-5f);
    }
  }
  // The one picked, and not the other monster.
  EXPECT_EQ(targets, 1);
  EXPECT_EQ(quests, 1);
}
