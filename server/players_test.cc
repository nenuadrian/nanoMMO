#include "server/players.h"

#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "server/accounts.h"
#include "server/test_services.h"

namespace {

class PlayersTest : public ::testing::Test {
 protected:
  void SetUp() override {
    database_ = nanommo::testing::TestDatabase();
    if (!database_) {
      GTEST_SKIP() << "set NANOMMO_MYSQL_HOST to test against MySQL";
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
  }

  std::unique_ptr<nanommo::Database> database_;
  const std::string name_ = nanommo::testing::TestName();
  uint32_t id_ = 0;
};

}  // namespace

TEST_F(PlayersTest, StartsANewPlayerInTheMiddle) {
  const absl::StatusOr<nanommo::Player> player =
      nanommo::Players(*database_).Load(id_);
  ASSERT_TRUE(player.ok());

  EXPECT_EQ(player->name, name_);
  EXPECT_EQ(player->x, 0.0f);
  EXPECT_EQ(player->z, 0.0f);
}

TEST_F(PlayersTest, LoadsWhereThePlayerWasSaved) {
  nanommo::Players players(*database_);
  ASSERT_TRUE(players.Save(id_, {.x = 2.5f, .z = -4.0f}).ok());
  ASSERT_TRUE(
      players.Save(id_, {.x = 3.5f, .z = -1.0f, .experience = 40}).ok());

  const absl::StatusOr<nanommo::Player> player = players.Load(id_);
  ASSERT_TRUE(player.ok());
  EXPECT_EQ(player->x, 3.5f);
  EXPECT_EQ(player->z, -1.0f);
  EXPECT_EQ(player->experience, 40u);
}
