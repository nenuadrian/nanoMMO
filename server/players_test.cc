#include "server/players.h"

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "protocol/nanommo.pb.h"
#include "server/accounts.h"
#include "server/test_services.h"

namespace {

class PlayersTest : public ::testing::Test {
 protected:
  void SetUp() override {
    database_ = nanommo::testing::TestDatabase();
    redis_ = nanommo::testing::TestRedis();
    if (!database_ || !redis_) {
      GTEST_SKIP() << "set NANOMMO_MYSQL_HOST and NANOMMO_REDIS_HOST to test "
                      "against MySQL and Redis";
    }
    const absl::StatusOr<uint32_t> id =
        nanommo::Accounts(*database_).Create(name_, "hash");
    ASSERT_TRUE(id.ok());
    id_ = *id;
  }

  void TearDown() override {
    if (database_) {
      EXPECT_TRUE(
          database_->Run("DELETE FROM accounts WHERE username = ?", {name_})
              .ok());
    }
    if (redis_) {
      const std::string id = std::to_string(id_);
      for (const std::string& key :
           {"player:Ember:" + id, "player:Frost:" + id, "player:" + id}) {
        EXPECT_TRUE(redis_->Command({"DEL", key}).ok());
      }
      EXPECT_TRUE(redis_->Command({"DEL", "player:Frost:0"}).ok());
      EXPECT_TRUE(
          redis_
              ->Command({"SREM", "players:unsaved", "player:Ember:" + id,
                         "player:Frost:" + id, id, "player:Frost:0"})
              .ok());
      EXPECT_TRUE(redis_
                      ->Command({"SREM", "items:unsaved", "player:Ember:" + id,
                                 "player:Frost:" + id})
                      .ok());
    }
  }

  std::unique_ptr<nanommo::Database> database_;
  std::unique_ptr<nanommo::Redis> redis_;
  const std::string name_ = nanommo::testing::TestName();
  uint32_t id_ = 0;
};

// Twelve bones in the bag, and a blade in hand that rolled three more damage.
nanommo::protocol::Inventory Bag(uint32_t revision) {
  nanommo::protocol::Inventory bag;
  nanommo::items::Stack& bones = (*bag.mutable_bag())[0];
  bones.set_item(3);
  bones.set_count(12);
  nanommo::items::Stack& blade =
      (*bag.mutable_worn())[nanommo::items::SLOT_MAIN_HAND];
  blade.set_item(6);
  blade.set_count(1);
  nanommo::items::Modifier* roll = blade.add_rolls();
  roll->set_stat(nanommo::items::STAT_DAMAGE);
  roll->set_amount(3);
  bag.set_revision(revision);
  return bag;
}

bool Same(const nanommo::protocol::Inventory& a,
          const nanommo::protocol::Inventory& b) {
  return google::protobuf::util::MessageDifferencer::Equals(a, b);
}

}  // namespace

TEST_F(PlayersTest, StartsANewPlayerInTheMiddle) {
  const absl::StatusOr<nanommo::Player> player =
      nanommo::Players(*database_, *redis_).Load("Ember", id_);
  ASSERT_TRUE(player.ok());

  EXPECT_EQ(player->name, name_);
  EXPECT_EQ(player->x, 0.0f);
  EXPECT_EQ(player->z, 0.0f);
}

TEST_F(PlayersTest, LoadsWhereThePlayerWasSavedInThatWorld) {
  nanommo::Players players(*database_, *redis_);
  ASSERT_TRUE(players.Save("Ember", {{id_, {.x = 2.5f, .z = -4.0f}}}).ok());
  const nanommo::Progress later = {.x = 3.5f,
                                   .z = -1.0f,
                                   .experience = 40,
                                   .health = 35,
                                   .land = 1,
                                   .weapon = 1,
                                   .skills = {1, 3}};
  ASSERT_TRUE(players.Save("Ember", {{id_, later}}).ok());

  const absl::StatusOr<nanommo::Player> player = players.Load("Ember", id_);
  ASSERT_TRUE(player.ok());
  EXPECT_EQ(player->x, 3.5f);
  EXPECT_EQ(player->z, -1.0f);
  EXPECT_EQ(player->experience, 40u);
  EXPECT_EQ(player->health, 35);
  EXPECT_EQ(player->land, 1);
  EXPECT_EQ(player->skills, (std::set<uint32_t>{1, 3}));

  const absl::StatusOr<nanommo::Player> elsewhere = players.Load("Frost", id_);
  ASSERT_TRUE(elsewhere.ok());
  EXPECT_EQ(elsewhere->x, 0.0f);
  EXPECT_EQ(elsewhere->experience, 0u);
}

TEST_F(PlayersTest, FlushesThePlayersToMySQL) {
  nanommo::Players players(*database_, *redis_);
  const nanommo::Progress progress = {.x = 1.5f,
                                      .z = 2.0f,
                                      .experience = 70,
                                      .health = 60,
                                      .land = 1,
                                      .weapon = 1,
                                      .skills = {1, 2}};
  ASSERT_TRUE(players.Save("Frost", {{id_, progress}}).ok());
  // No account has the id zero, so MySQL refuses this one, and it holds back
  // nobody.
  ASSERT_TRUE(players.Save("Frost", {{0, {.x = 1.0f}}}).ok());
  // A player saved before worlds: an account ID in the set, and its hash.
  const std::string id = std::to_string(id_);
  ASSERT_TRUE(redis_
                  ->Pipeline({{"HSET", "player:" + id, "x", "3", "z", "4",
                               "experience", "5", "health", "6"},
                              {"SADD", "players:unsaved", id}})
                  .ok());

  const absl::StatusOr<int> saved = players.Flush();
  ASSERT_TRUE(saved.ok());
  EXPECT_GE(*saved, 2);
  const absl::StatusOr<nanommo::Result> rows = database_->Run(
      "SELECT world, x, z, experience, health, land, weapon FROM players "
      "WHERE account_id = ? ORDER BY world",
      {id});
  ASSERT_TRUE(rows.ok());
  EXPECT_EQ(rows->rows,
            (std::vector<std::vector<std::string>>{
                {"Ember", "3", "4", "5", "6", "Meadows", ""},
                {"Frost", "1.5", "2", "70", "60", "Ashlands", "1"}}));

  // With Redis's copy gone, the skills come back from MySQL's rows.
  ASSERT_TRUE(redis_->Command({"DEL", "player:Frost:" + id}).ok());
  const absl::StatusOr<nanommo::Player> player = players.Load("Frost", id_);
  ASSERT_TRUE(player.ok());
  EXPECT_EQ(player->skills, (std::set<uint32_t>{1, 2}));
}

TEST_F(PlayersTest, LoadsTheBagTheyLeftWith) {
  nanommo::Players players(*database_, *redis_);
  // Never saved, the bag has no revision, which the world takes as a new
  // player's.
  const absl::StatusOr<nanommo::Player> before = players.Load("Ember", id_);
  ASSERT_TRUE(before.ok());
  EXPECT_EQ(before->inventory.revision(), 0u);

  ASSERT_TRUE(
      players.Save("Ember", {{id_, {.items_revision = 2}}}, {{id_, Bag(2)}})
          .ok());
  // A save without the bag, as it hasn't changed, leaves the one Redis has.
  ASSERT_TRUE(
      players.Save("Ember", {{id_, {.x = 1.0f, .items_revision = 2}}}).ok());
  const absl::StatusOr<nanommo::Player> player = players.Load("Ember", id_);
  ASSERT_TRUE(player.ok());
  EXPECT_EQ(player->x, 1.0f);
  EXPECT_TRUE(Same(player->inventory.items(), Bag(2)))
      << player->inventory.items().DebugString();
}

TEST_F(PlayersTest, FlushesTheBagsThatChangedToMySQL) {
  nanommo::Players players(*database_, *redis_);
  const std::string id = std::to_string(id_);
  const auto rows = [&] {
    return database_
        ->Run(
            "SELECT place, item, count, state FROM player_items "
            "WHERE account_id = ? AND world = 'Ember' ORDER BY place",
            {id})
        ->rows;
  };
  ASSERT_TRUE(
      players.Save("Ember", {{id_, {.items_revision = 2}}}, {{id_, Bag(2)}})
          .ok());
  ASSERT_TRUE(players.Flush().ok());
  EXPECT_EQ(rows(),
            (std::vector<std::vector<std::string>>{
                {"bag 0", "3", "12", ""},
                {"main hand", "6", "1",
                 R"({"rolls": [{"stat": "STAT_DAMAGE", "amount": 3}]})"}}));

  // With Redis's copy gone, the bag comes back from the rows.
  ASSERT_TRUE(redis_->Command({"DEL", "player:Ember:" + id}).ok());
  absl::StatusOr<nanommo::Player> player = players.Load("Ember", id_);
  ASSERT_TRUE(player.ok());
  EXPECT_TRUE(Same(player->inventory.items(), Bag(2)))
      << player->inventory.items().DebugString();

  // A save of where they are alone doesn't write the rows again: one we
  // change by hand stays as it is.
  ASSERT_TRUE(database_
                  ->Run("UPDATE player_items SET count = 7 WHERE account_id = "
                        "? AND place = 'bag 0'",
                        {id})
                  .ok());
  ASSERT_TRUE(
      players.Save("Ember", {{id_, {.x = 2.0f, .items_revision = 2}}}).ok());
  ASSERT_TRUE(players.Flush().ok());
  EXPECT_EQ(rows().front()[2], "7");

  // A bag that's emptied empties its rows, and keeps its revision, so that
  // nobody takes it for a new player's.
  nanommo::protocol::Inventory empty;
  empty.set_revision(3);
  ASSERT_TRUE(
      players.Save("Ember", {{id_, {.items_revision = 3}}}, {{id_, empty}})
          .ok());
  ASSERT_TRUE(players.Flush().ok());
  EXPECT_TRUE(rows().empty());
  ASSERT_TRUE(redis_->Command({"DEL", "player:Ember:" + id}).ok());
  player = players.Load("Ember", id_);
  ASSERT_TRUE(player.ok());
  EXPECT_EQ(player->inventory.revision(), 3u);
  EXPECT_TRUE(player->inventory.items().bag().empty());
}

TEST_F(PlayersTest, FlushesMoreRowsOfBagsThanAStatementTakes) {
  // Forty players, each with a stack in every slot of the bag, and an item
  // in every slot that's worn: more rows than go in one statement.
  constexpr int kPlayers = 40;
  std::vector<uint32_t> ids;
  nanommo::Accounts accounts(*database_);
  for (int i = 0; i < kPlayers; ++i) {
    const absl::StatusOr<uint32_t> id =
        accounts.Create(nanommo::testing::TestName(), "hash");
    ASSERT_TRUE(id.ok()) << id.status();
    ids.push_back(*id);
  }
  nanommo::protocol::Inventory full;
  for (uint32_t slot = 0; slot < nanommo::Inventory::kBagSlots; ++slot) {
    (*full.mutable_bag())[slot].set_item(3);
    (*full.mutable_bag())[slot].set_count(slot + 1);
  }
  for (int worn = 1; worn <= 5; ++worn) {
    (*full.mutable_worn())[worn].set_item(1);
    (*full.mutable_worn())[worn].set_count(1);
  }
  full.set_revision(1);
  std::map<uint32_t, nanommo::Progress> changed;
  std::map<uint32_t, nanommo::protocol::Inventory> bags;
  for (const uint32_t id : ids) {
    changed[id] = {.items_revision = 1};
    bags[id] = full;
  }
  nanommo::Players players(*database_, *redis_);
  ASSERT_TRUE(players.Save("Ember", changed, bags).ok());
  ASSERT_GT(kPlayers * (nanommo::Inventory::kBagSlots + 5),
            nanommo::Players::kItemRows);
  ASSERT_TRUE(players.Flush().ok());

  std::vector<std::string> keys = {"DEL"};
  for (const uint32_t id : ids) {
    const absl::StatusOr<nanommo::Result> rows =
        database_->Run("SELECT COUNT(*) FROM player_items WHERE account_id = ?",
                       {std::to_string(id)});
    ASSERT_TRUE(rows.ok()) << rows.status();
    EXPECT_EQ(rows->rows.front().front(),
              std::to_string(nanommo::Inventory::kBagSlots + 5));
    keys.push_back("player:Ember:" + std::to_string(id));
    EXPECT_TRUE(
        database_
            ->Run("DELETE FROM accounts WHERE id = ?", {std::to_string(id)})
            .ok());
  }
  EXPECT_TRUE(redis_->Command(keys).ok());
}

TEST_F(PlayersTest, KeepsTheQuestsTheyreOnAndHaveDone) {
  nanommo::Players players(*database_, *redis_);
  const std::string id = std::to_string(id_);
  const nanommo::Progress progress = {.quests = {{2, 3}}, .quests_done = {1}};
  ASSERT_TRUE(players.Save("Ember", {{id_, progress}}).ok());
  absl::StatusOr<nanommo::Player> player = players.Load("Ember", id_);
  ASSERT_TRUE(player.ok());
  EXPECT_EQ(player->quests, (std::map<uint32_t, uint32_t>{{2, 3}}));
  EXPECT_EQ(player->quests_done, (std::set<uint32_t>{1}));

  // MySQL keeps them as JSON, with the player's row.
  ASSERT_TRUE(players.Flush().ok());
  const absl::StatusOr<nanommo::Result> rows = database_->Run(
      "SELECT quests FROM players WHERE account_id = ? AND world = 'Ember'",
      {id});
  ASSERT_TRUE(rows.ok());
  EXPECT_EQ(rows->rows, (std::vector<std::vector<std::string>>{
                            {R"({"on": {"2": 3}, "done": [1]})"}}));

  // So with Redis's copy gone, they come back from there.
  ASSERT_TRUE(redis_->Command({"DEL", "player:Ember:" + id}).ok());
  player = players.Load("Ember", id_);
  ASSERT_TRUE(player.ok());
  EXPECT_EQ(player->quests, (std::map<uint32_t, uint32_t>{{2, 3}}));
  EXPECT_EQ(player->quests_done, (std::set<uint32_t>{1}));
}
