#include "server/token_bucket.h"

#include <chrono>

#include "gtest/gtest.h"

namespace nanommo {
namespace {

using std::chrono::seconds;

TEST(TokenBucket, LetsABurstThroughThenOneEachRefill) {
  TokenBucket bucket(5, seconds(2));
  const std::chrono::steady_clock::time_point start;
  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(bucket.Take(start));
  }
  EXPECT_FALSE(bucket.Take(start));
  EXPECT_FALSE(bucket.Take(start + seconds(1)));
  EXPECT_TRUE(bucket.Take(start + seconds(2)));

  // A long wait fills the bucket, and no more.
  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(bucket.Take(start + seconds(60)));
  }
  EXPECT_FALSE(bucket.Take(start + seconds(60)));
}

}  // namespace
}  // namespace nanommo
