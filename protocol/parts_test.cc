#include "protocol/parts.h"

#include <cstdint>
#include <string>

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

}  // namespace

TEST(Parts, SplitsAWorldIntoDatagrams) {
  const auto parts = nanommo::Split(MakeWorld(200, 1'800'000'000'000'000));

  ASSERT_GT(parts.size(), 1u);
  nanommo::protocol::World you;
  you.set_you(4'000'000'000);
  you.set_experience(4'000'000'000);
  you.set_next_level(4'000'000'000);
  int players = 0;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    const std::string bytes =
        parts[i].SerializeAsString() + you.SerializeAsString();
    EXPECT_LE(bytes.size(), nanommo::kMaxDatagramSize);
    EXPECT_EQ(parts[i].part(), i);
    EXPECT_EQ(parts[i].parts(), parts.size());
    EXPECT_EQ(parts[i].sent(), 1'800'000'000'000'000);
    players += parts[i].players_size();
  }
  EXPECT_EQ(players, 200);
}

TEST(Parts, LeavesThePlayersOfALostPartWhereTheyWere) {
  nanommo::protocol::World first = MakeWorld(200, 1);
  for (uint32_t id = 1; id <= 3; ++id) {
    first.add_monsters()->set_id(id);
  }
  nanommo::Parts assembled;
  for (const auto& part : nanommo::Split(first)) {
    assembled.Add(part);
  }
  const auto next = nanommo::Split(MakeWorld(200, 2, 0.5f));
  for (std::size_t i = 1; i < next.size(); ++i) {
    assembled.Add(next[i]);
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
