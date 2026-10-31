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
  EXPECT_FALSE(loading.covering());
}

TEST(Loading, IsCoveredOnlyOnceACoveredFrameIsDrawn) {
  nanommo::Loading loading;
  loading.Cover();
  EXPECT_EQ(loading.cover(), 1.0f);
  EXPECT_FALSE(loading.covered());

  loading.Update(kFrame);
  EXPECT_TRUE(loading.covered());
  // Asked again while it's up, as each frame without the world does.
  loading.Cover();
  EXPECT_TRUE(loading.covered());
}

TEST(Loading, FadesOnceTheLandIsBuilt) {
  nanommo::Loading loading;
  loading.Cover();
  loading.Update(kFrame);
  loading.Lift();
  EXPECT_FALSE(loading.covering());

  // The frame that built the land took long, and counts as a short one.
  loading.Update(2.0f);
  EXPECT_GT(loading.cover(), 0.5f);

  Wait(loading, nanommo::Loading::kFade);
  EXPECT_EQ(loading.cover(), 0.0f);
}
