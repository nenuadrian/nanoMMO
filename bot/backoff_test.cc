#include "bot/backoff.h"

#include <chrono>
#include <random>
#include <set>

#include "gtest/gtest.h"

namespace {

using std::chrono::milliseconds;

TEST(Backoff, DoublesWithEachFailureUpToThirtySeconds) {
  std::mt19937 random(1);
  for (int i = 0; i < 100; ++i) {
    const milliseconds first = nanommo::Backoff(0, random);
    EXPECT_GE(first, milliseconds(500));
    EXPECT_LE(first, milliseconds(1000));
    const milliseconds third = nanommo::Backoff(2, random);
    EXPECT_GE(third, milliseconds(2000));
    EXPECT_LE(third, milliseconds(4000));
    const milliseconds later = nanommo::Backoff(40, random);
    EXPECT_GE(later, milliseconds(15000));
    EXPECT_LE(later, milliseconds(30000));
  }
}

TEST(Backoff, SpreadsBotsThatFailedTogether) {
  std::mt19937 random(1);
  std::set<milliseconds::rep> waits;
  for (int bot = 0; bot < 100; ++bot) {
    waits.insert(nanommo::Backoff(3, random).count());
  }
  // A hundred bots, dropped together, come back over four seconds, not at
  // once.
  EXPECT_GT(waits.size(), 90u);
  EXPECT_GE(*waits.begin(), 4000);
  EXPECT_LE(*waits.rbegin(), 8000);
  EXPECT_GT(*waits.rbegin() - *waits.begin(), 3000);
}

}  // namespace
