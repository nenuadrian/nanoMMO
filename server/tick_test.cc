#include "server/tick.h"

#include <chrono>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gtest/gtest.h"

namespace nanommo {
namespace {

TEST(TickInterval, TwentyTicksIsFiftyMilliseconds) {
  const absl::StatusOr<std::chrono::nanoseconds> interval = TickInterval(20);
  ASSERT_TRUE(interval.ok());
  EXPECT_EQ(*interval, std::chrono::milliseconds(50));
}

TEST(TickInterval, KeepsNanosecondPrecision) {
  const absl::StatusOr<std::chrono::nanoseconds> interval = TickInterval(60);
  ASSERT_TRUE(interval.ok());
  EXPECT_EQ(*interval, std::chrono::nanoseconds(16'666'666));
}

TEST(TickInterval, RejectsNonPositiveRates) {
  EXPECT_EQ(TickInterval(0).status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(TickInterval(-20).status().code(),
            absl::StatusCode::kInvalidArgument);
}

using std::chrono::milliseconds;
using std::chrono::steady_clock;

TEST(NextTick, ComesAnIntervalAfterTheOneBefore) {
  const steady_clock::time_point due = steady_clock::now();
  // However long into its 50 milliseconds the tick took.
  EXPECT_EQ(NextTick(due, due + milliseconds(3), milliseconds(50)),
            due + milliseconds(50));
}

TEST(NextTick, CatchesUpOnATickThatRanLate) {
  const steady_clock::time_point due = steady_clock::now();
  // The tick took 300 milliseconds, so the next is due already, and the
  // five after it come back to back.
  EXPECT_EQ(NextTick(due, due + milliseconds(300), milliseconds(50)),
            due + milliseconds(50));
}

TEST(NextTick, LetsTheTicksOfAStallGo) {
  const steady_clock::time_point due = steady_clock::now();
  // The tick waited five seconds for Redis: the hundred ticks it missed
  // aren't run back to back.
  const steady_clock::time_point now = due + std::chrono::seconds(5);
  EXPECT_EQ(NextTick(due, now, milliseconds(50)), now + milliseconds(50));
  // As a second behind is still caught up on.
  EXPECT_EQ(NextTick(due, due + milliseconds(1050), milliseconds(50)),
            due + milliseconds(50));
}

}  // namespace
}  // namespace nanommo
