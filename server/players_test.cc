#include "server/players.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "protocol/nanommo.pb.h"
#include "server/accounts.h"
#include "server/characters.h"
#include "server/inventory.h"
#include "server/test_services.h"
#include "server/tokens.h"
#include "server/tracing.h"
#include "server/wars.h"

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
    const absl::StatusOr<uint32_t> account =
        nanommo::Accounts(*database_).Create(name_, "hash");
    ASSERT_TRUE(account.ok());
    account_ = *account;
    // A character in Ember, named after the account, and one in Frost.
    nanommo::Characters characters(*database_);
    const absl::StatusOr<uint32_t> id =
        characters.Create(account_, "Ember", name_);
    const absl::StatusOr<uint32_t> frost =
        characters.Create(account_, "Frost", name_);
    ASSERT_TRUE(id.ok()) << id.status();
    ASSERT_TRUE(frost.ok()) << frost.status();
    id_ = *id;
    frost_ = *frost;
  }

  void TearDown() override {
    if (database_) {
      EXPECT_TRUE(
          database_->Run("DELETE FROM accounts WHERE username = ?", {name_})
              .ok());
    }
    if (redis_) {
      const std::string ember = "player:Ember:" + std::to_string(id_);
      const std::string frost = "player:Frost:" + std::to_string(frost_);
      for (const std::string& key :
           {ember, frost, std::string("player:Frost:0")}) {
        EXPECT_TRUE(redis_->Command({"DEL", key}).ok());
      }
      EXPECT_TRUE(redis_
                      ->Command({"SREM", "players:unsaved", ember, frost,
                                 "player:Frost:0"})
                      .ok());
      EXPECT_TRUE(
          redis_->Command({"SREM", "items:unsaved", ember, frost}).ok());
      EXPECT_TRUE(
          redis_->Command({"SREM", "players:saving", ember, frost}).ok());
      EXPECT_TRUE(redis_->Command({"SREM", "items:saving", ember, frost}).ok());
      EXPECT_TRUE(redis_->Command({"DEL", Lease()}).ok());
    }
  }

  std::string Lease() const { return "playing:Ember:" + std::to_string(id_); }

  std::unique_ptr<nanommo::Database> database_;
  std::unique_ptr<nanommo::Redis> redis_;
  const std::string name_ = nanommo::testing::TestName();
  uint32_t account_ = 0;
  // The account's characters, in Ember and in Frost.
  uint32_t id_ = 0;
  uint32_t frost_ = 0;
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

// A lease's fencing number, which comes after its instance.
uint64_t Fence(const std::string& lease) {
  uint64_t fence = 0;
  return absl::SimpleAtoi(lease.substr(lease.rfind(' ') + 1), &fence) ? fence
                                                                      : 0;
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

  // Ember's character is no player of Frost's, and the one in Frost is new.
  EXPECT_TRUE(absl::IsNotFound(players.Load("Frost", id_).status()));
  const absl::StatusOr<nanommo::Player> elsewhere =
      players.Load("Frost", frost_);
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
  ASSERT_TRUE(players.Save("Frost", {{frost_, progress}}).ok());
  // No character has the id zero, so there's no row to update, and it holds
  // back nobody.
  ASSERT_TRUE(players.Save("Frost", {{0, {.x = 1.0f}}}).ok());
  // A player saved by its account, before characters, says which account
  // and not which character, so it's left: Ember's row stays as it was.
  const std::string ember = "player:Ember:" + std::to_string(id_);
  ASSERT_TRUE(redis_
                  ->Pipeline({{"HSET", ember, "world", "Ember", "account",
                               std::to_string(account_), "x", "3", "z", "4"},
                              {"SADD", "players:unsaved", ember}})
                  .ok());

  const absl::StatusOr<int> saved = players.Flush();
  ASSERT_TRUE(saved.ok()) << saved.status();
  EXPECT_GE(*saved, 1);
  const absl::StatusOr<nanommo::Result> rows = database_->Run(
      "SELECT world, x, z, experience, health, land, weapon FROM players "
      "WHERE account_id = ? ORDER BY world",
      {std::to_string(account_)});
  ASSERT_TRUE(rows.ok());
  EXPECT_EQ(rows->rows,
            (std::vector<std::vector<std::string>>{
                {"Ember", "0", "0", "0", "", "Meadows", ""},
                {"Frost", "1.5", "2", "70", "60", "Ashlands", "1"}}));

  // With Redis's copy gone, the skills come back from MySQL's rows.
  ASSERT_TRUE(
      redis_->Command({"DEL", "player:Frost:" + std::to_string(frost_)}).ok());
  const absl::StatusOr<nanommo::Player> player = players.Load("Frost", frost_);
  ASSERT_TRUE(player.ok());
  EXPECT_EQ(player->skills, (std::set<uint32_t>{1, 2}));
}

TEST_F(PlayersTest, CountsThePlayersWaitingForAFlush) {
  nanommo::Players players(*database_, *redis_);
  const absl::StatusOr<int> before = players.Unsaved();
  ASSERT_TRUE(before.ok()) << before.status();
  ASSERT_TRUE(players.Save("Ember", {{id_, {.x = 1.0f}}}).ok());
  EXPECT_EQ(players.Unsaved().value_or(0), *before + 1);

  ASSERT_TRUE(players.Flush().ok());
  EXPECT_EQ(players.Unsaved().value_or(-1), 0);
}

TEST_F(PlayersTest, ABatchASaverDiedWithIsSavedByTheNextFlush) {
  nanommo::Players players(*database_, *redis_);
  const std::string key = "player:Frost:" + std::to_string(frost_);
  ASSERT_TRUE(
      players
          .Save("Frost",
                {{frost_, {.x = 4.5f, .experience = 30, .items_revision = 3}}},
                {{frost_, Bag(3)}})
          .ok());
  // As a flush that took them leaves them, when its saver's killed as it
  // waits for MySQL: in its batch's sets, and no longer marked unsaved.
  ASSERT_TRUE(
      redis_
          ->Pipeline({{"SMOVE", "players:unsaved", "players:saving", key},
                      {"SMOVE", "items:unsaved", "items:saving", key}})
          .ok());
  // They're still waiting.
  EXPECT_GE(players.Unsaved().value_or(0), 1);

  ASSERT_TRUE(players.Flush().ok());
  const absl::StatusOr<nanommo::Result> row = database_->Run(
      "SELECT x, experience, (SELECT COUNT(*) FROM player_items WHERE player "
      "= players.id) FROM players WHERE id = ?",
      {std::to_string(frost_)});
  ASSERT_TRUE(row.ok()) << row.status();
  EXPECT_EQ(row->rows,
            (std::vector<std::vector<std::string>>{{"4.5", "30", "2"}}));
  EXPECT_EQ(players.Unsaved().value_or(-1), 0);
}

TEST_F(PlayersTest, CountsThePlayersInABatchAndMarkedAgainOnce) {
  nanommo::Players players(*database_, *redis_);
  const std::string key = "player:Frost:" + std::to_string(frost_);
  const absl::StatusOr<int> before = players.Unsaved();
  ASSERT_TRUE(before.ok()) << before.status();
  // Taken into a batch, as a flush that waits for MySQL has them, and then
  // changed, so marked unsaved again: in both sets.
  ASSERT_TRUE(players.Save("Frost", {{frost_, {.x = 1.0f}}}).ok());
  ASSERT_TRUE(
      redis_->Command({"SMOVE", "players:unsaved", "players:saving", key})
          .ok());
  ASSERT_TRUE(players.Save("Frost", {{frost_, {.x = 2.0f}}}).ok());

  EXPECT_EQ(players.Unsaved().value_or(0), *before + 1);
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
            "WHERE player = ? ORDER BY place",
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
                  ->Run("UPDATE player_items SET count = 7 WHERE player = ? "
                        "AND place = 'bag 0'",
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

TEST_F(PlayersTest, KeepsSerials) {
  nanommo::Players players(*database_, *redis_);
  // All 64 bits of it, through MySQL's JSON as well as Redis.
  nanommo::protocol::Inventory bag = Bag(2);
  (*bag.mutable_worn())[nanommo::items::SLOT_MAIN_HAND].set_serial(
      0xFEDCBA9876543210);
  ASSERT_TRUE(
      players.Save("Ember", {{id_, {.items_revision = 2}}}, {{id_, bag}}).ok());
  absl::StatusOr<nanommo::Player> player = players.Load("Ember", id_);
  ASSERT_TRUE(player.ok()) << player.status();
  EXPECT_TRUE(Same(player->inventory.items(), bag))
      << player->inventory.items().DebugString();

  ASSERT_TRUE(players.Flush().ok());
  ASSERT_TRUE(
      redis_->Command({"DEL", "player:Ember:" + std::to_string(id_)}).ok());
  player = players.Load("Ember", id_);
  ASSERT_TRUE(player.ok()) << player.status();
  EXPECT_TRUE(Same(player->inventory.items(), bag))
      << player->inventory.items().DebugString();
}

TEST_F(PlayersTest, FlushesMoreRowsOfBagsThanAStatementTakes) {
  // Forty players, each with a stack in every slot of the bag, and an item
  // in every slot that's worn: more rows than go in one statement.
  constexpr int kPlayers = 40;
  std::vector<uint32_t> accounts_made;
  std::vector<uint32_t> ids;
  nanommo::Accounts accounts(*database_);
  nanommo::Characters characters(*database_);
  for (int i = 0; i < kPlayers; ++i) {
    const std::string name = nanommo::testing::TestName();
    const absl::StatusOr<uint32_t> account = accounts.Create(name, "hash");
    ASSERT_TRUE(account.ok()) << account.status();
    accounts_made.push_back(*account);
    const absl::StatusOr<uint32_t> id =
        characters.Create(*account, "Ember", name);
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
        database_->Run("SELECT COUNT(*) FROM player_items WHERE player = ?",
                       {std::to_string(id)});
    ASSERT_TRUE(rows.ok()) << rows.status();
    EXPECT_EQ(rows->rows.front().front(),
              std::to_string(nanommo::Inventory::kBagSlots + 5));
    keys.push_back("player:Ember:" + std::to_string(id));
  }
  for (const uint32_t account : accounts_made) {
    EXPECT_TRUE(database_
                    ->Run("DELETE FROM accounts WHERE id = ?",
                          {std::to_string(account)})
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
  const absl::StatusOr<nanommo::Result> rows =
      database_->Run("SELECT quests FROM players WHERE id = ?", {id});
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

TEST_F(PlayersTest, KeepsTheFactionForGood) {
  nanommo::Players players(*database_, *redis_);
  const std::string id = std::to_string(id_);
  absl::StatusOr<nanommo::Player> player = players.Load("Ember", id_);
  ASSERT_TRUE(player.ok());
  EXPECT_EQ(player->faction, 0u);

  // Sworn, they're saved into Redis with it, and the saver takes it to MySQL.
  ASSERT_TRUE(players.Save("Ember", {{id_, {.faction = 2}}}).ok());
  player = players.Load("Ember", id_);
  ASSERT_TRUE(player.ok());
  EXPECT_EQ(player->faction, 2u);
  ASSERT_TRUE(players.Flush().ok());
  const auto faction = [&] {
    return database_->Run("SELECT faction FROM players WHERE id = ?", {id})
        .value_or(nanommo::Result())
        .rows;
  };
  EXPECT_EQ(faction(), (std::vector<std::vector<std::string>>{{"2"}}));

  // A save with another, or with none, doesn't switch it in MySQL, which the
  // player comes back with once Redis's copy is gone.
  ASSERT_TRUE(players.Save("Ember", {{id_, {.faction = 3}}}).ok());
  ASSERT_TRUE(players.Flush().ok());
  ASSERT_TRUE(players.Save("Ember", {{id_, {.x = 1.0f}}}).ok());
  ASSERT_TRUE(players.Flush().ok());
  EXPECT_EQ(faction(), (std::vector<std::vector<std::string>>{{"2"}}));
  ASSERT_TRUE(redis_->Command({"DEL", "player:Ember:" + id}).ok());
  player = players.Load("Ember", id_);
  ASSERT_TRUE(player.ok());
  EXPECT_EQ(player->faction, 2u);
}

TEST_F(PlayersTest, SavesGoldWithTheBag) {
  nanommo::Players players(*database_, *redis_);
  const std::string id = std::to_string(id_);
  nanommo::protocol::Inventory bag = Bag(2);
  bag.set_gold(120);
  ASSERT_TRUE(
      players.Save("Ember", {{id_, {.items_revision = 2}}}, {{id_, bag}}).ok());
  // Redis has it in the bag's bytes.
  absl::StatusOr<nanommo::Player> player = players.Load("Ember", id_);
  ASSERT_TRUE(player.ok());
  EXPECT_EQ(player->inventory.gold(), 120u);

  // MySQL keeps it in the player's row, with the bag's revision.
  ASSERT_TRUE(players.Flush().ok());
  const absl::StatusOr<nanommo::Result> rows = database_->Run(
      "SELECT items_revision, gold FROM players WHERE id = ?", {id});
  ASSERT_TRUE(rows.ok());
  EXPECT_EQ(rows->rows, (std::vector<std::vector<std::string>>{{"2", "120"}}));

  // A save of where they are alone leaves it, and with Redis's copy gone, it
  // comes back from there.
  ASSERT_TRUE(
      players.Save("Ember", {{id_, {.x = 2.0f, .items_revision = 2}}}).ok());
  ASSERT_TRUE(players.Flush().ok());
  ASSERT_TRUE(redis_->Command({"DEL", "player:Ember:" + id}).ok());
  player = players.Load("Ember", id_);
  ASSERT_TRUE(player.ok());
  EXPECT_EQ(player->inventory.gold(), 120u);
  EXPECT_TRUE(Same(player->inventory.items(), bag))
      << player->inventory.items().DebugString();
}

TEST_F(PlayersTest, AnInstanceThatLostThePlayerCantSaveThem) {
  nanommo::Players players(*database_, *redis_);
  const absl::StatusOr<std::string> first =
      players.Lease("Ember", id_, "Ember-7777");
  ASSERT_TRUE(first.ok()) << first.status();
  EXPECT_TRUE(absl::IsFailedPrecondition(
      players.Lease("Ember", id_, "Ember-7877").status()));
  const auto saved = [&](float x, const std::string& lease) {
    return players.Save("Ember", {{id_, {.x = x}}}, {}, {{id_, lease}})
        .value_or(std::set<uint32_t>{0});
  };
  EXPECT_TRUE(saved(1.0f, *first).empty());

  // The first stalls for longer than a lease, which runs out, and the second
  // takes them, with a higher fencing number.
  ASSERT_TRUE(redis_->Command({"DEL", Lease()}).ok());
  const absl::StatusOr<std::string> second =
      players.Lease("Ember", id_, "Ember-7877");
  ASSERT_TRUE(second.ok()) << second.status();
  EXPECT_GT(Fence(*second), Fence(*first));

  // The first can't save them over what the second saves, and the second can.
  EXPECT_EQ(saved(2.0f, *first), std::set<uint32_t>{id_});
  absl::StatusOr<nanommo::Player> player = players.Load("Ember", id_);
  ASSERT_TRUE(player.ok()) << player.status();
  EXPECT_EQ(player->x, 1.0f);
  EXPECT_TRUE(saved(3.0f, *second).empty());
  player = players.Load("Ember", id_);
  ASSERT_TRUE(player.ok()) << player.status();
  EXPECT_EQ(player->x, 3.0f);
}

TEST_F(PlayersTest, AFlushTriedTwiceRecordsATradeOnce) {
  nanommo::Players players(*database_, *redis_);
  // A second character in Ember, to whom the first gives their blade.
  const absl::StatusOr<uint32_t> other =
      nanommo::Characters(*database_).Create(account_, "Ember", name_ + "b");
  ASSERT_TRUE(other.ok()) << other.status();
  nanommo::protocol::Inventory giver = Bag(3);
  nanommo::items::Stack& blade =
      giver.mutable_worn()->at(nanommo::items::SLOT_MAIN_HAND);
  blade.set_serial(7);
  nanommo::protocol::Inventory taker;
  taker.set_revision(2);
  (*taker.mutable_bag())[0] = blade;
  nanommo::Traded trade{.id = name_, .player = id_, .partner = *other};
  (*trade.player_gave.mutable_bag())[0] = blade;
  giver.mutable_worn()->erase(nanommo::items::SLOT_MAIN_HAND);
  const auto save = [&] {
    return players.Save(
        "Ember",
        {{id_, {.items_revision = 3}}, {*other, {.items_revision = 2}}},
        {{id_, giver}, {*other, taker}}, {}, trade);
  };

  // The batch goes into MySQL, which commits it. Should the saver not hear
  // so, as when the connection drops, the trade's still in the stream, and
  // its players unsaved, as another save of it leaves them, and it tries the
  // batch again.
  ASSERT_TRUE(save().ok());
  ASSERT_TRUE(players.Flush().ok());
  ASSERT_TRUE(save().ok());
  ASSERT_TRUE(players.Flush().ok());
  const absl::StatusOr<nanommo::Result> recorded = database_->Run(
      "SELECT player, partner, player_gave->>'$.bag.\"0\".serial' "
      "FROM trades WHERE id = ?",
      {name_});
  ASSERT_TRUE(recorded.ok()) << recorded.status();
  EXPECT_EQ(recorded->rows,
            (std::vector<std::vector<std::string>>{
                {std::to_string(id_), std::to_string(*other), "7"}}));
  // And the blade with both bags, in the partner's.
  const absl::StatusOr<nanommo::Result> blades = database_->Run(
      "SELECT player FROM player_items WHERE serial = 7 AND player IN (?, ?)",
      {std::to_string(id_), std::to_string(*other)});
  ASSERT_TRUE(blades.ok()) << blades.status();
  EXPECT_EQ(blades->rows,
            (std::vector<std::vector<std::string>>{{std::to_string(*other)}}));

  const std::string key = "player:Ember:" + std::to_string(*other);
  EXPECT_TRUE(redis_->Command({"DEL", key}).ok());
  EXPECT_TRUE(database_->Run("DELETE FROM trades WHERE id = ?", {name_}).ok());
}

TEST_F(PlayersTest, TheSaverCarriesOnATradesTrace) {
  nanommo::Players players(*database_, *redis_);
  const absl::StatusOr<uint32_t> other =
      nanommo::Characters(*database_).Create(account_, "Ember", name_ + "c");
  ASSERT_TRUE(other.ok()) << other.status();
  // Each span, as it's sent.
  std::mutex mutex;
  std::vector<nlohmann::json> spans;
  nanommo::StartTracing("test", [&](const std::string& json) {
    const nlohmann::json batch = nlohmann::json::parse(json, nullptr, false);
    const std::lock_guard lock(mutex);
    for (const nlohmann::json& span :
         batch["resourceSpans"][0]["scopeSpans"][0]["spans"]) {
      spans.push_back(span);
    }
    return absl::OkStatus();
  });
  {
    const nanommo::Span trade =
        nanommo::StartSpan("trade", nanommo::SpanKind::kServer);
    EXPECT_TRUE(
        players
            .Save(
                "Ember", {{id_, {.x = 1.0f}}, {*other, {.x = 2.0f}}}, {}, {},
                nanommo::Traded{.id = name_, .player = id_, .partner = *other})
            .ok());
  }
  // The saver's flush isn't a request, and is traced only for the trade.
  EXPECT_TRUE(players.Flush().ok());
  nanommo::StopTracing();

  const auto named = [&spans](std::string_view name) {
    for (const nlohmann::json& span : spans) {
      if (span["name"] == name) {
        return span;
      }
    }
    return nlohmann::json::object();
  };
  const nlohmann::json published = named("publish trade");
  const nlohmann::json saved = named("save trades");
  ASSERT_TRUE(published.contains("kind")) << "no span published the trade";
  ASSERT_TRUE(saved.contains("kind")) << "no span saved the trade";
  EXPECT_EQ(published["kind"], 4);
  EXPECT_EQ(saved["kind"], 5);
  EXPECT_EQ(saved["parentSpanId"], published["spanId"]);
  EXPECT_EQ(saved["traceId"], named("trade")["traceId"]);
  // And its record into MySQL, in the same trace.
  EXPECT_EQ(named("mysql INSERT trades")["traceId"], saved["traceId"]);

  const std::string key = "player:Ember:" + std::to_string(*other);
  EXPECT_TRUE(redis_->Command({"DEL", key}).ok());
  EXPECT_TRUE(database_->Run("DELETE FROM trades WHERE id = ?", {name_}).ok());
}

// Two guilds in Ember, named after the test's account, at war, and the
// account's character in Ember killing the one in Frost for either side.
class WarKillsTest : public PlayersTest {
 protected:
  void SetUp() override {
    PlayersTest::SetUp();
    if (IsSkipped() || HasFatalFailure()) {
      return;
    }
    for (const char* side : {"a", "b"}) {
      const absl::StatusOr<nanommo::Result> guild =
          database_->Run("INSERT INTO guilds (world, name) VALUES ('Ember', ?)",
                         {name_ + side});
      ASSERT_TRUE(guild.ok()) << guild.status();
      guilds_.push_back(std::to_string(guild->insert_id));
    }
    const absl::StatusOr<nanommo::Result> started = database_->Run(
        "INSERT INTO guild_wars (world, challenger, challenged, ends_at) "
        "VALUES ('Ember', ?, ?, NOW() + INTERVAL 30 MINUTE)",
        guilds_);
    ASSERT_TRUE(started.ok()) << started.status();
    war_ = static_cast<uint32_t>(started->insert_id);
  }

  void TearDown() override {
    if (database_) {
      EXPECT_TRUE(database_
                      ->Run("DELETE FROM guilds WHERE world = 'Ember' AND "
                            "name IN (?, ?)",
                            {name_ + "a", name_ + "b"})
                      .ok());
    }
    PlayersTest::TearDown();
  }

  // A kill for the challenger's side, 0, or the other's, 1, with an id of
  // its own, as the server gives it.
  nanommo::WarKill Kill(int side) const {
    uint32_t guild = 0;
    EXPECT_TRUE(absl::SimpleAtoi(guilds_[side], &guild));
    return {.id = nanommo::RandomHex(),
            .war = war_,
            .killer = id_,
            .victim = frost_,
            .guild = guild};
  }

  // Each side's score, as MySQL has it: the war's rows that are its.
  std::vector<std::string> Score() const {
    const absl::StatusOr<nanommo::Result> score = database_->Run(
        "SELECT (SELECT COUNT(*) FROM war_kills WHERE war = ? AND guild = ?), "
        "(SELECT COUNT(*) FROM war_kills WHERE war = ? AND guild = ?)",
        {std::to_string(war_), guilds_[0], std::to_string(war_), guilds_[1]});
    EXPECT_TRUE(score.ok()) << score.status();
    return score.ok() ? score->rows.front() : std::vector<std::string>();
  }

  // The war's kills in the stream, which the saver hasn't taken out yet.
  std::vector<nanommo::Redis::Entry> Waiting() const {
    std::vector<nanommo::Redis::Entry> ours;
    for (nanommo::Redis::Entry& entry :
         redis_->Read({"XREAD", "COUNT", "1000", "STREAMS", "war_kills", "0-0"})
             .value_or(std::vector<nanommo::Redis::Entry>())) {
      if (std::ranges::find(entry.fields, std::to_string(war_)) !=
          entry.fields.end()) {
        ours.push_back(std::move(entry));
      }
    }
    return ours;
  }

  std::vector<std::string> guilds_;
  uint32_t war_ = 0;
};

TEST_F(WarKillsTest, AFlushAddsTheWarKillsToEachSidesScore) {
  nanommo::Players players(*database_, *redis_);
  // The challenger's side kills twice, in a save with a player's, and the
  // other side once, in a save of its own.
  ASSERT_TRUE(players
                  .Save("Ember", {{id_, {.x = 1.0f}}}, {}, {}, std::nullopt,
                        {Kill(0), Kill(0)})
                  .ok());
  ASSERT_TRUE(players.Save("Ember", {}, {}, {}, std::nullopt, {Kill(1)}).ok());
  ASSERT_TRUE(players.Flush().ok());
  EXPECT_EQ(Score(), (std::vector<std::string>{"2", "1"}));
  // And they're out of the stream.
  EXPECT_TRUE(Waiting().empty());
}

// The saver commits a batch's kills, and can't take them out of the stream,
// as when Redis hangs in between, so it reads them again: they score once.
TEST_F(WarKillsTest, KillsTheSaverReadsAgainScoreOnce) {
  nanommo::Players players(*database_, *redis_);
  ASSERT_TRUE(
      players.Save("Ember", {}, {}, {}, std::nullopt, {Kill(0), Kill(1)}).ok());
  const std::vector<nanommo::Redis::Entry> read = Waiting();
  ASSERT_EQ(read.size(), 2u);
  ASSERT_TRUE(players.Flush().ok());
  ASSERT_EQ(Score(), (std::vector<std::string>{"1", "1"}));

  for (const nanommo::Redis::Entry& kill : read) {
    std::vector<std::string> again = {"XADD", "war_kills", "*"};
    again.insert(again.end(), kill.fields.begin(), kill.fields.end());
    ASSERT_TRUE(redis_->Command(again).ok());
  }
  ASSERT_TRUE(players.Flush().ok());
  EXPECT_EQ(Score(), (std::vector<std::string>{"1", "1"}));
  EXPECT_TRUE(Waiting().empty());
}

TEST_F(PlayersTest, CountsTheSerialsHeldTwice) {
  nanommo::Players players(*database_, *redis_);
  const absl::StatusOr<int> before = players.Duplicates();
  ASSERT_TRUE(before.ok()) << before.status();

  // The same blade, by its serial, in the account's characters in Ember and
  // in Frost.
  nanommo::protocol::Inventory bag = Bag(2);
  bag.mutable_worn()
      ->at(nanommo::items::SLOT_MAIN_HAND)
      .set_serial(nanommo::NewSerial());
  ASSERT_TRUE(
      players.Save("Ember", {{id_, {.items_revision = 2}}}, {{id_, bag}}).ok());
  ASSERT_TRUE(
      players.Save("Frost", {{frost_, {.items_revision = 2}}}, {{frost_, bag}})
          .ok());
  ASSERT_TRUE(players.Flush().ok());
  EXPECT_EQ(players.Duplicates().value_or(0), *before + 1);
}
