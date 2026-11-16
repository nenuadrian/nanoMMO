#include "server/inputs.h"

#include <chrono>
#include <cstdint>

#include "gtest/gtest.h"
#include "protocol/clock.h"
#include "protocol/nanommo.pb.h"
#include "server/world.h"

namespace {

nanommo::protocol::Input Keys(uint32_t sequence, float x) {
  nanommo::protocol::Input input;
  input.set_sequence(sequence);
  input.set_x(x);
  return input;
}

}  // namespace

TEST(Inputs, NumberedKeysWaitForTheirTick) {
  nanommo::World world;
  world.Join(7, {.name = "alice"});
  nanommo::Inputs inputs;

  inputs.Take(world, 7, Keys(1, 1.0f));
  EXPECT_FLOAT_EQ(world.players().at(7).input_x, 0.0f);
  EXPECT_FALSE(inputs.acked(7).has_value());

  inputs.Apply([&world](uint32_t) { return &world; });
  EXPECT_FLOAT_EQ(world.players().at(7).input_x, 1.0f);
  EXPECT_EQ(inputs.acked(7), 1u);

  inputs.Leave(7);
  EXPECT_FALSE(inputs.acked(7).has_value());
}

TEST(Inputs, KeysNotNumberedGoAtOnceUntilNumberedOnesCome) {
  nanommo::World world;
  world.Join(7, {.name = "alice"});
  nanommo::Inputs inputs;

  inputs.Take(world, 7, Keys(0, 1.0f));
  EXPECT_FLOAT_EQ(world.players().at(7).input_x, 1.0f);

  // Once the client numbers its keys, one that isn't is a copy of the last,
  // and the numbered ones decide.
  inputs.Take(world, 7, Keys(1, 0.5f));
  inputs.Take(world, 7, Keys(0, -1.0f));
  EXPECT_FLOAT_EQ(world.players().at(7).input_x, 1.0f);
  inputs.Apply([&world](uint32_t) { return &world; });
  EXPECT_FLOAT_EQ(world.players().at(7).input_x, 0.5f);
}

TEST(Inputs, DrainsAQueueABurstLeftFull) {
  nanommo::World world;
  world.Join(7, {.name = "alice"});
  nanommo::Inputs inputs;

  // Three at once, and then one a tick, as a client keeps up after a slow
  // frame: two wait behind each new one, until the queue drains them.
  uint32_t sequence = 0;
  for (; sequence < 3; ++sequence) {
    inputs.Take(world, 7, Keys(sequence + 1, 0.0f));
  }
  for (int tick = 0; tick < 3 * nanommo::Inputs::kSteadyTicks; ++tick) {
    inputs.Apply([&world](uint32_t) { return &world; });
    inputs.Take(world, 7, Keys(++sequence, 0.0f));
  }
  // The newest sent before the tick is the one it applies.
  inputs.Apply([&world](uint32_t) { return &world; });
  EXPECT_EQ(inputs.acked(7), sequence);
}

TEST(Inputs, AClaimFurtherBackThanTheMostIsTheMost) {
  nanommo::World world;
  world.Join(7, {.name = "alice"});
  nanommo::Inputs inputs;
  uint32_t sequence = 0;
  // How far behind the world takes alice's client to be, from an input that
  // says it drew everyone at the time, in microseconds on our clock.
  const auto behind = [&](int64_t drawn) {
    nanommo::protocol::Input input = Keys(++sequence, 0.0f);
    input.set_drawn(drawn);
    inputs.Take(world, 7, input);
    inputs.Apply([&world](uint32_t) { return &world; });
    return world.players().at(7).behind;
  };

  // 150 milliseconds back is believed, give or take the moment it took.
  const std::chrono::duration<double> believed =
      behind(nanommo::NowMicros() - 150'000);
  EXPECT_NEAR(believed.count(), 0.15, 0.01);
  // A second back is the most, 200 milliseconds, and a time ahead of ours is
  // now, as is an input that doesn't say, as a bot's.
  EXPECT_EQ(behind(nanommo::NowMicros() - 1'000'000),
            nanommo::Inputs::kMostRewind);
  EXPECT_EQ(behind(nanommo::NowMicros() + 1'000'000),
            std::chrono::nanoseconds::zero());
  EXPECT_EQ(behind(0), std::chrono::nanoseconds::zero());
}
