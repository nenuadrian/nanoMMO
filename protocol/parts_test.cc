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
  you.set_respawn(4'000'000'000);
  you.mutable_crafting()->set_recipe(4'000'000'000);
  you.mutable_crafting()->set_done(4'000'000'000);
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
  full.mutable_players(0)->set_guild("Iron Wolves");
  full.mutable_players(0)->set_faction(2);
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
  EXPECT_EQ(result.players(0).guild(), "Iron Wolves");
  EXPECT_EQ(result.players(0).faction(), 2u);
  EXPECT_EQ(result.players(0).x(), 1.5f);

  // A world with their name, and no weapon or guild: they've put it away, and
  // left the guild.
  assembled.Add(MakeWorld(3, 3));
  EXPECT_EQ(assembled.world().players(0).weapon(), 0u);
  EXPECT_EQ(assembled.world().players(0).guild(), "");
}

TEST(Parts, TakesTheInputAckedFromTheNewestWorld) {
  nanommo::Parts assembled;
  nanommo::protocol::World first = MakeWorld(1, 1);
  first.set_acked(7);
  assembled.Add(first);
  nanommo::protocol::World second = MakeWorld(1, 2);
  second.set_acked(8);
  assembled.Add(second);
  EXPECT_EQ(assembled.world().acked(), 8u);
}

TEST(Parts, KeepsYourCooldownsAndSkills) {
  nanommo::protocol::World world = MakeWorld(1, 1);
  (*world.mutable_cooldowns())[2] = 1500;
  world.add_skills(1);
  world.add_skills(2);
  nanommo::Parts assembled;
  assembled.Add(world);

  const nanommo::protocol::World result = assembled.world();
  ASSERT_TRUE(result.cooldowns().contains(2));
  EXPECT_EQ(result.cooldowns().at(2), 1500u);
  EXPECT_EQ(
      std::vector<uint32_t>(result.skills().begin(), result.skills().end()),
      (std::vector<uint32_t>{1, 2}));
}

TEST(Parts, TakesYourWaitToComeBackFromTheNewestWorld) {
  nanommo::protocol::World dead = MakeWorld(1, 1);
  dead.set_respawn(7000);
  nanommo::Parts assembled;
  assembled.Add(dead);
  EXPECT_EQ(assembled.world().respawn(), 7000u);

  // Back, with no wait in the world, as protobuf leaves out a zero.
  assembled.Add(MakeWorld(1, 2));
  EXPECT_EQ(assembled.world().respawn(), 0u);
}

TEST(Parts, TakesWhatYoureMakingFromTheNewestWorld) {
  nanommo::protocol::World making = MakeWorld(1, 1);
  making.mutable_crafting()->set_recipe(2);
  making.mutable_crafting()->set_done(4000);
  nanommo::Parts assembled;
  assembled.Add(making);
  EXPECT_EQ(assembled.world().crafting().recipe(), 2u);
  EXPECT_EQ(assembled.world().crafting().done(), 4000u);

  // Made, or stopped, it's nothing.
  assembled.Add(MakeWorld(1, 2));
  EXPECT_EQ(assembled.world().crafting().recipe(), 0u);
}

TEST(Parts, TakesTheSkyFromTheNewestWorld) {
  nanommo::protocol::World first = MakeWorld(1, 1);
  first.mutable_sky()->set_day(0.5f);
  first.mutable_sky()->set_weather(nanommo::protocol::WEATHER_RAIN);
  first.mutable_sky()->set_strength(0.8f);
  nanommo::Parts assembled;
  assembled.Add(first);
  EXPECT_FLOAT_EQ(assembled.world().sky().day(), 0.5f);
  EXPECT_EQ(assembled.world().sky().weather(), nanommo::protocol::WEATHER_RAIN);

  nanommo::protocol::World second = MakeWorld(1, 2);
  second.mutable_sky()->set_day(0.51f);
  assembled.Add(second);
  EXPECT_FLOAT_EQ(assembled.world().sky().day(), 0.51f);
  EXPECT_EQ(assembled.world().sky().weather(),
            nanommo::protocol::WEATHER_CLEAR);
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

TEST(Parts, KeepsTheStillAsTheyWereAndSaysWhichWorldCameWhole) {
  nanommo::Parts assembled;
  nanommo::protocol::World first = MakeWorld(3, 1);
  assembled.Add(first);
  EXPECT_EQ(assembled.whole(), 1);

  // A whole world with player 1 in it and the others still: none of them go.
  nanommo::protocol::World next = MakeWorld(1, 2, 5.0f);
  next.add_still_players(next.players(0).id() + 1);
  next.add_still_players(next.players(0).id() + 2);
  assembled.Add(next);
  const nanommo::protocol::World result = assembled.world();
  ASSERT_EQ(result.players_size(), 3);
  EXPECT_EQ(result.players(1).x(), first.players(1).x());
  EXPECT_EQ(assembled.whole(), 2);
}
