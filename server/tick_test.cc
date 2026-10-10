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

}  // namespace
}  // namespace nanommo
