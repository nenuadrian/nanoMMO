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
  player->set_yaw(1.25f);
  player->set_motion(nanommo::protocol::MOTION_RUN);
  world.add_monsters()->set_kind(1);

  const std::string bytes = world.SerializeAsString();

  nanommo::protocol::World received;
  ASSERT_TRUE(received.ParseFromString(bytes));
  EXPECT_EQ(received.you(), 2u);
  ASSERT_EQ(received.players_size(), 1);
  EXPECT_EQ(received.players(0).id(), 2u);
  EXPECT_EQ(received.players(0).x(), 1.5f);
  EXPECT_EQ(received.players(0).z(), -3.0f);
  EXPECT_EQ(received.players(0).yaw(), 1.25f);
  EXPECT_EQ(received.players(0).motion(), nanommo::protocol::MOTION_RUN);
  ASSERT_EQ(received.monsters_size(), 1);
  EXPECT_EQ(received.monsters(0).kind(), 1u);
}

TEST(Protocol, ReadsMessagesBackToBackAsOne) {
  nanommo::protocol::World players;
  players.add_players()->set_id(7);
  nanommo::protocol::World you;
  you.set_you(7);
  you.set_experience(40);

  const std::string bytes =
      players.SerializeAsString() + you.SerializeAsString();

  nanommo::protocol::World received;
  ASSERT_TRUE(received.ParseFromString(bytes));
  EXPECT_EQ(received.you(), 7u);
  EXPECT_EQ(received.experience(), 40u);
  ASSERT_EQ(received.players_size(), 1);
  EXPECT_EQ(received.players(0).id(), 7u);
}

TEST(Protocol, ZeroesAreNotSent) {
  nanommo::protocol::Input input;
  input.set_x(1.0f);

  EXPECT_EQ(input.SerializeAsString().size(), 5u);
}
