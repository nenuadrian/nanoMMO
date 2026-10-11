#include "server/accounts.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "server/test_services.h"

TEST(AccountRules, AcceptsGoodNamesOnly) {
  EXPECT_TRUE(nanommo::CheckUsername("alice_7").ok());
  EXPECT_FALSE(nanommo::CheckUsername("ab").ok());
  EXPECT_FALSE(nanommo::CheckUsername("seventeen_letters").ok());
  EXPECT_FALSE(nanommo::CheckUsername("alice smith").ok());
  EXPECT_FALSE(nanommo::CheckUsername("alice';--").ok());
}

TEST(AccountRules, RefusesShortPasswords) {
  EXPECT_FALSE(nanommo::CheckPassword("hunter2").ok());
  EXPECT_TRUE(nanommo::CheckPassword("correct horse").ok());
}

namespace {

class AccountsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    database_ = nanommo::testing::TestDatabase();
    if (!database_) {
      GTEST_SKIP() << "set NANOMMO_MYSQL_HOST to test against MySQL";
    }
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
};

}  // namespace

TEST_F(AccountsTest, FindsTheAccountItCreated) {
  nanommo::Accounts accounts(*database_);
  const absl::StatusOr<uint32_t> id = accounts.Create(name_, "hash");
  ASSERT_TRUE(id.ok());

  const absl::StatusOr<nanommo::Account> found = accounts.Find(name_);
  ASSERT_TRUE(found.ok());
  EXPECT_EQ(found->id, *id);
  EXPECT_EQ(found->password_hash, "hash");
}

TEST_F(AccountsTest, RefusesANameThatIsTaken) {
  nanommo::Accounts accounts(*database_);
  ASSERT_TRUE(accounts.Create(name_, "hash").ok());

  EXPECT_TRUE(
      absl::IsAlreadyExists(accounts.Create(name_, "another hash").status()));
  EXPECT_TRUE(absl::IsAlreadyExists(
      accounts.Create("TEST" + name_.substr(4), "another hash").status()));
}

TEST_F(AccountsTest, ReconnectsAfterMySqlDropsAnIdleConnection) {
  ASSERT_TRUE(database_->Run("SET SESSION wait_timeout = 1").ok());
  std::this_thread::sleep_for(std::chrono::seconds(2));

  EXPECT_TRUE(nanommo::Accounts(*database_).Create(name_, "hash").ok());
}
