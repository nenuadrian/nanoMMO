#include <string>

#include "gtest/gtest.h"
#include "protocol/nanommo.pb.h"

TEST(Protocol, WorldSurvivesTheRoundTrip) {
  nanommo::protocol::World world;
  world.set_you(2);
  nanommo::protocol::Player* player = world.add_players();
  player->set_id(2);
  player->set_x(1.5f);
  player->set_z(-3.0f);

  const std::string bytes = world.SerializeAsString();

  nanommo::protocol::World received;
  ASSERT_TRUE(received.ParseFromString(bytes));
  EXPECT_EQ(received.you(), 2u);
  ASSERT_EQ(received.players_size(), 1);
  EXPECT_EQ(received.players(0).id(), 2u);
  EXPECT_EQ(received.players(0).x(), 1.5f);
  EXPECT_EQ(received.players(0).z(), -3.0f);
}

TEST(Protocol, ZeroesAreNotSent) {
  nanommo::protocol::Input input;
  input.set_x(1.0f);

  EXPECT_EQ(input.SerializeAsString().size(), 5u);
}
