#include "server/world.h"

#include <chrono>
#include <cmath>
#include <limits>

#include "gtest/gtest.h"

namespace {

constexpr std::chrono::milliseconds kTick(50);

void RunOneSecond(nanommo::World& world) {
  for (int tick = 0; tick < 20; ++tick) {
    world.Step(kTick);
  }
}

}  // namespace

TEST(World, MovesFourTilesASecond) {
  nanommo::World world;
  const uint32_t id = world.Join();
  const float start = world.players().at(id).x;

  world.SetInput(id, 1.0f, 0.0f);
  RunOneSecond(world);

  EXPECT_FLOAT_EQ(world.players().at(id).x, start + 4.0f);
}

TEST(World, DiagonalsAreNoFaster) {
  nanommo::World world;
  const uint32_t id = world.Join();
  const nanommo::Player start = world.players().at(id);

  world.SetInput(id, 1.0f, 1.0f);
  RunOneSecond(world);

  const nanommo::Player& end = world.players().at(id);
  EXPECT_FLOAT_EQ(std::hypot(end.x - start.x, end.z - start.z), 4.0f);
}

TEST(World, KeepsPlayersOnTheGround) {
  nanommo::World world;
  const uint32_t id = world.Join();

  world.SetInput(id, 0.0f, -1.0f);
  for (int second = 0; second < 10; ++second) {
    RunOneSecond(world);
  }

  EXPECT_EQ(world.players().at(id).z, -nanommo::World::kEdge);
}

TEST(World, IgnoresInputThatIsNotANumber) {
  nanommo::World world;
  const uint32_t id = world.Join();
  const float start = world.players().at(id).x;

  world.SetInput(id, std::numeric_limits<float>::quiet_NaN(), 0.0f);
  RunOneSecond(world);

  EXPECT_EQ(world.players().at(id).x, start);
}
