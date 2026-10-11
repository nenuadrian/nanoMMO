#include "server/login_server.h"

#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "server/test_services.h"

namespace {

class LoginTest : public ::testing::Test {
 protected:
  void SetUp() override {
    database_ = nanommo::testing::TestDatabase();
    redis_ = nanommo::testing::TestRedis();
    if (!database_ || !redis_) {
      GTEST_SKIP()
          << "set NANOMMO_MYSQL_HOST and NANOMMO_REDIS_HOST to test logins";
    }
    accounts_ = std::make_unique<nanommo::Accounts>(*database_);
    tokens_ = std::make_unique<nanommo::Tokens>(*redis_);
  }

  void TearDown() override {
    if (database_) {
      EXPECT_TRUE(
          database_->Run("DELETE FROM accounts WHERE username = ?", {name_})
              .ok());
    }
  }

  nanommo::protocol::LoginReply LogIn(const std::string& password,
                                      bool new_account = false) {
    nanommo::protocol::LoginRequest request;
    request.set_username(name_);
    request.set_password(password);
    request.set_new_account(new_account);
    absl::StatusOr<nanommo::protocol::LoginReply> reply =
        nanommo::HandleLogin(request, *accounts_, *tokens_);
    EXPECT_TRUE(reply.ok()) << reply.status();
    return reply.value_or(nanommo::protocol::LoginReply());
  }

  std::unique_ptr<nanommo::Database> database_;
  std::unique_ptr<nanommo::Redis> redis_;
  std::unique_ptr<nanommo::Accounts> accounts_;
  std::unique_ptr<nanommo::Tokens> tokens_;
  const std::string name_ = nanommo::testing::TestName();
};

}  // namespace

TEST_F(LoginTest, ANewAccountGetsAToken) {
  const auto reply = LogIn("correct horse", true);

  EXPECT_TRUE(reply.ok());
  EXPECT_TRUE(tokens_->Redeem(reply.token()).ok());
}

TEST_F(LoginTest, LogsInWithTheRightPasswordOnly) {
  ASSERT_TRUE(LogIn("correct horse", true).ok());
  EXPECT_TRUE(LogIn("correct horse").ok());

  const auto reply = LogIn("battery staple");
  EXPECT_FALSE(reply.ok());
  EXPECT_EQ(reply.error(), "Wrong name or password.");
  EXPECT_TRUE(reply.token().empty());
}

TEST_F(LoginTest, RefusesANameThatIsTaken) {
  ASSERT_TRUE(LogIn("correct horse", true).ok());

  EXPECT_EQ(LogIn("battery staple", true).error(), "That name is taken.");
}
