#include "client/loading.h"

#include "gtest/gtest.h"

namespace {

constexpr float kFrame = 1.0f / 60.0f;

void Wait(nanommo::Loading& loading, float seconds) {
  for (float time = 0.0f; time < seconds; time += kFrame) {
    loading.Update(kFrame);
  }
}

}  // namespace

TEST(Loading, ShowsTheWorldUntilAskedToCover) {
  nanommo::Loading loading;
  EXPECT_EQ(loading.cover(), 0.0f);
  EXPECT_EQ(loading.clear(), 1.0f);
  EXPECT_FALSE(loading.covering());
}

TEST(Loading, IsCoveredOnlyOnceACoveredFrameIsDrawn) {
  nanommo::Loading loading;
  loading.Cover();
  EXPECT_EQ(loading.cover(), 1.0f);
  EXPECT_EQ(loading.clear(), 0.0f);
  EXPECT_FALSE(loading.covered());

  loading.Update(kFrame);
  EXPECT_TRUE(loading.covered());
  // Asked again while it's up, as each frame without the world does.
  loading.Cover();
  EXPECT_TRUE(loading.covered());
}

TEST(Loading, FadesThenLiftsTheFogNearestFirst) {
  nanommo::Loading loading;
  loading.Cover();
  loading.Update(kFrame);
  loading.Lift();
  EXPECT_FALSE(loading.covering());

  // The frame that built the land took long, and counts as a short one.
  loading.Update(2.0f);
  EXPECT_GT(loading.cover(), 0.5f);
  EXPECT_LT(loading.clear(), 0.2f);

  Wait(loading, nanommo::Loading::kFade);
  EXPECT_EQ(loading.cover(), 0.0f);
  // The fog lifts fast at first, then slower.
  EXPECT_GT(loading.clear(), 0.5f);
  EXPECT_LT(loading.clear(), 1.0f);

  Wait(loading, nanommo::Loading::kLift);
  EXPECT_EQ(loading.clear(), 1.0f);
}

TEST(Loading, CoversAgainForTheNextLandWhileTheFogLifts) {
  nanommo::Loading loading;
  loading.Cover();
  loading.Update(kFrame);
  loading.Lift();
  Wait(loading, 0.5f);

  loading.Cover();
  EXPECT_EQ(loading.cover(), 1.0f);
  EXPECT_FALSE(loading.covered());
  loading.Update(kFrame);
  EXPECT_TRUE(loading.covered());
}
