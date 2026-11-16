#include "server/history.h"

#include <chrono>
#include <optional>

#include "gtest/gtest.h"

namespace nanommo {
namespace {

using std::chrono::milliseconds;

TEST(History, PlacesBetweenTwoTicks) {
  History history;
  history.Record(1, milliseconds(1000), {.x = 0.0f, .z = 2.0f});
  history.Record(1, milliseconds(1050), {.x = 1.0f, .z = 2.0f});

  // A fifth of the way from one tick to the next, a fifth of the way along.
  const std::optional<Place> then = history.At(1, milliseconds(1010));
  ASSERT_TRUE(then.has_value());
  EXPECT_FLOAT_EQ(then->x, 0.2f);
  EXPECT_FLOAT_EQ(then->z, 2.0f);
  // Nobody else has been anywhere.
  EXPECT_FALSE(history.At(2, milliseconds(1010)).has_value());
}

TEST(History, KeepsNothingOlderThanItsMost) {
  History history;
  // A metre a tick, for a second and a half.
  for (int tick = 0; tick <= 30; ++tick) {
    history.Record(1, milliseconds(50 * tick),
                   {.x = static_cast<float>(tick), .z = 0.0f});
  }

  // A second back from the newest is the oldest it has, at tick ten, and
  // anything before that is there too. After the newest, it's the newest.
  EXPECT_FLOAT_EQ(history.At(1, milliseconds(500))->x, 10.0f);
  EXPECT_FLOAT_EQ(history.At(1, milliseconds(0))->x, 10.0f);
  EXPECT_FLOAT_EQ(history.At(1, milliseconds(2000))->x, 30.0f);

  // A player who's left has none.
  history.Leave(1);
  EXPECT_FALSE(history.At(1, milliseconds(1500)).has_value());
}

}  // namespace
}  // namespace nanommo
