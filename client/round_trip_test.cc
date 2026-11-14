#include "client/round_trip.h"

#include "gtest/gtest.h"

TEST(RoundTrip, IsFromSendingAnInputToHearingItApplied) {
  nanommo::RoundTrip round_trip;
  EXPECT_EQ(round_trip.milliseconds(), 0.0f);
  round_trip.Sent(1, 10.0);
  round_trip.Sent(2, 10.05);
  round_trip.Acked(1, 10.15);
  EXPECT_FLOAT_EQ(round_trip.milliseconds(), 150.0f);

  // A later one moves it a tenth of the way, and one acked again doesn't.
  round_trip.Acked(2, 10.30);
  EXPECT_FLOAT_EQ(round_trip.milliseconds(), 160.0f);
  round_trip.Acked(2, 11.0);
  EXPECT_FLOAT_EQ(round_trip.milliseconds(), 160.0f);
}
