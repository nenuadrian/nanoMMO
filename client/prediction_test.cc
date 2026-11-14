#include "client/prediction.h"

#include "gtest/gtest.h"
#include "server/walking.h"
#include "terrain/terrain.h"

namespace {

constexpr float kTick = 0.05f;

}  // namespace

TEST(Prediction, WalksYouAtOnceOnceTheServerHasPlacedYou) {
  const nanommo::Terrain flat;
  nanommo::Prediction prediction;
  prediction.Correct(flat, 0.0f, 0.0f, 0, kTick);
  prediction.Walk(flat, 1, 1.0f, 0.0f, false, kTick);
  EXPECT_FLOAT_EQ(prediction.x(), nanommo::kWalkSpeed * kTick);
  EXPECT_EQ(prediction.pending(), 1u);
}

TEST(Prediction, WalksAgainThroughWhatTheServerHasntApplied) {
  const nanommo::Terrain flat;
  nanommo::Prediction prediction;
  prediction.Correct(flat, 0.0f, 0.0f, 0, kTick);
  for (uint32_t sequence = 1; sequence <= 3; ++sequence) {
    prediction.Walk(flat, sequence, 1.0f, 0.0f, false, kTick);
  }
  // The server agrees about the first: we're where we were.
  prediction.Correct(flat, nanommo::kWalkSpeed * kTick, 0.0f, 1, kTick);
  EXPECT_FLOAT_EQ(prediction.x(), 3 * nanommo::kWalkSpeed * kTick);
  EXPECT_EQ(prediction.pending(), 2u);
  EXPECT_FLOAT_EQ(prediction.shown_x(), prediction.x());
}

TEST(Prediction, TheServerWinsAndTheDifferenceFades) {
  const nanommo::Terrain flat;
  nanommo::Prediction prediction;
  prediction.Correct(flat, 0.0f, 0.0f, 0, kTick);
  for (uint32_t sequence = 1; sequence <= 3; ++sequence) {
    prediction.Walk(flat, sequence, 1.0f, 0.0f, false, kTick);
  }
  // Something held us back for the first tick on the server.
  prediction.Correct(flat, 0.0f, 0.0f, 1, kTick);
  EXPECT_FLOAT_EQ(prediction.x(), 2 * nanommo::kWalkSpeed * kTick);
  // Drawn where we were, at first, and then where the server says.
  EXPECT_FLOAT_EQ(prediction.shown_x(), 3 * nanommo::kWalkSpeed * kTick);
  prediction.Settle(1.0f);
  EXPECT_NEAR(prediction.shown_x(), prediction.x(), 1e-6f);
}

TEST(Prediction, AJumpIsntFaded) {
  const nanommo::Terrain flat;
  nanommo::Prediction prediction;
  prediction.Correct(flat, 0.0f, 0.0f, 0, kTick);
  prediction.Correct(flat, 40.0f, 0.0f, 0, kTick);
  EXPECT_FLOAT_EQ(prediction.shown_x(), 40.0f);
}
