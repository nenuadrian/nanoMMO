#include "protocol/parts.h"

#include <cstdint>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace {

nanommo::protocol::World MakeWorld(int players, int64_t sent,
                                   float shift = 0.0f) {
  nanommo::protocol::World world;
  world.set_sent(sent);
  for (int i = 1; i <= players; ++i) {
    nanommo::protocol::Player* player = world.add_players();
    player->set_id(static_cast<uint32_t>(i));
    player->set_name("bot" + std::to_string(i));
    player->set_x(static_cast<float>(i) + shift);
    player->set_yaw(1.25f);
    player->set_motion(nanommo::protocol::MOTION_RUN);
  }
  return world;
}

// The world's players and monsters, each in a piece, split into parts.
std::vector<std::string> Split(const nanommo::protocol::World& world) {
  std::vector<std::string> pieces;
  for (const nanommo::protocol::Player& player : world.players()) {
    pieces.push_back(nanommo::Piece(player));
  }
  for (const nanommo::protocol::Monster& monster : world.monsters()) {
    pieces.push_back(nanommo::Piece(monster));
  }
  return nanommo::Split({pieces.begin(), pieces.end()}, world.sent());
}

nanommo::protocol::World Parse(const std::string& bytes) {
  nanommo::protocol::World world;
  EXPECT_TRUE(world.ParseFromString(bytes));
  return world;
}

}  // namespace

TEST(Parts, SplitsAWorldIntoDatagrams) {
  const std::vector<std::string> parts =
      Split(MakeWorld(200, 1'800'000'000'000'000));

  ASSERT_GT(parts.size(), 1u);
  nanommo::protocol::World you;
  you.set_you(4'000'000'000);
  you.set_experience(4'000'000'000);
  you.set_next_level(4'000'000'000);
  you.set_seed(4'000'000'000);
  you.set_land(4'000'000'000);
  for (uint32_t skill = 1; skill <= 9; ++skill) {
    (*you.mutable_cooldowns())[skill] = 4'000'000'000;
  }
  int players = 0;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    EXPECT_LE(parts[i].size() + you.ByteSizeLong(), nanommo::kMaxDatagramSize);
    const nanommo::protocol::World part = Parse(parts[i]);
    EXPECT_EQ(part.part(), i);
    EXPECT_EQ(part.parts(), parts.size());
    EXPECT_EQ(part.sent(), 1'800'000'000'000'000);
    players += part.players_size();
  }
  EXPECT_EQ(players, 200);
}

TEST(Parts, LeavesThePlayersOfALostPartWhereTheyWere) {
  nanommo::protocol::World first = MakeWorld(200, 1);
  for (uint32_t id = 1; id <= 3; ++id) {
    first.add_monsters()->set_id(id);
  }
  nanommo::Parts assembled;
  for (const std::string& part : Split(first)) {
    assembled.Add(Parse(part));
  }
  const std::vector<std::string> next = Split(MakeWorld(200, 2, 0.5f));
  for (std::size_t i = 1; i < next.size(); ++i) {
    assembled.Add(Parse(next[i]));
  }

  const nanommo::protocol::World result = assembled.world();
  ASSERT_EQ(result.players_size(), 200);
  EXPECT_EQ(result.players(0).x(), 1.0f);
  EXPECT_EQ(result.players(199).x(), 200.5f);
  EXPECT_EQ(result.monsters_size(), 3);
}

TEST(Parts, DropsPlayersWhoLeftAndPartsThatArriveLate) {
  nanommo::Parts assembled;
  assembled.Add(MakeWorld(3, 1));
  assembled.Add(MakeWorld(2, 3));
  assembled.Add(MakeWorld(3, 2));

  EXPECT_EQ(assembled.world().players_size(), 2);
}

TEST(Parts, KeepsNamesAndLevelsBetweenTheTicksThatSendThem) {
  nanommo::protocol::World full = MakeWorld(3, 1);
  full.mutable_players(0)->set_level(4);
  full.mutable_players(0)->set_weapon(1);
  nanommo::protocol::World compact = MakeWorld(3, 2, 0.5f);
  for (nanommo::protocol::Player& player : *compact.mutable_players()) {
    player.clear_name();
  }
  nanommo::Parts assembled;
  assembled.Add(full);
  assembled.Add(compact);

  const nanommo::protocol::World result = assembled.world();
  ASSERT_EQ(result.players_size(), 3);
  EXPECT_EQ(result.players(0).name(), "bot1");
  EXPECT_EQ(result.players(0).level(), 4u);
  EXPECT_EQ(result.players(0).weapon(), 1u);
  EXPECT_EQ(result.players(0).x(), 1.5f);
}

TEST(Parts, KeepsYourCooldowns) {
  nanommo::protocol::World world = MakeWorld(1, 1);
  (*world.mutable_cooldowns())[2] = 1500;
  nanommo::Parts assembled;
  assembled.Add(world);

  const nanommo::protocol::World result = assembled.world();
  ASSERT_TRUE(result.cooldowns().contains(2));
  EXPECT_EQ(result.cooldowns().at(2), 1500u);
}

TEST(Parts, KeepsNpcsBetweenTheWorldsThatSendThemUntilTheyAreGone) {
  constexpr int64_t kSecond = 1'000'000;
  nanommo::protocol::World full = MakeWorld(1, kSecond);
  full.add_npcs()->set_id(1);
  full.mutable_npcs(0)->set_kind(1);
  nanommo::Parts assembled;
  assembled.Add(full);

  // The next full world, a second later, was lost.
  assembled.Add(MakeWorld(1, 3 * kSecond));
  ASSERT_EQ(assembled.world().npcs_size(), 1);
  EXPECT_EQ(assembled.world().npcs(0).kind(), 1u);

  assembled.Add(MakeWorld(1, 5 * kSecond));
  EXPECT_EQ(assembled.world().npcs_size(), 0);
}
