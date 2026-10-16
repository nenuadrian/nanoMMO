#include "server/players.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"
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
    }
  }

  std::unique_ptr<nanommo::Database> database_;
  std::unique_ptr<nanommo::Redis> redis_;
  const std::string name_ = nanommo::testing::TestName();
  uint32_t id_ = 0;
};

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
                                   .weapon = 1};
  ASSERT_TRUE(players.Save("Ember", {{id_, later}}).ok());

  const absl::StatusOr<nanommo::Player> player = players.Load("Ember", id_);
  ASSERT_TRUE(player.ok());
  EXPECT_EQ(player->x, 3.5f);
  EXPECT_EQ(player->z, -1.0f);
  EXPECT_EQ(player->experience, 40u);
  EXPECT_EQ(player->health, 35);
  EXPECT_EQ(player->land, 1);
  EXPECT_EQ(player->weapon, 1u);

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
                                      .weapon = 1};
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
}
