#include "server/login_server.h"

#include <memory>
#include <string>
#include <string_view>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "protocol/version.h"
#include "server/characters.h"
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
    characters_ = std::make_unique<nanommo::Characters>(*database_);
  }

  void TearDown() override {
    if (database_) {
      EXPECT_TRUE(
          database_->Run("DELETE FROM accounts WHERE username = ?", {name_})
              .ok());
    }
  }

  nanommo::protocol::LoginReply Ask(
      const nanommo::protocol::LoginRequest& request) {
    absl::StatusOr<nanommo::protocol::LoginReply> reply =
        nanommo::HandleLogin(request, *accounts_, *tokens_, *characters_);
    EXPECT_TRUE(reply.ok()) << reply.status();
    return reply.value_or(nanommo::protocol::LoginReply());
  }

  nanommo::protocol::LoginReply LogIn(
      const std::string& password, bool new_account = false,
      std::string_view version = nanommo::kVersion) {
    nanommo::protocol::LoginRequest request;
    request.set_username(name_);
    request.set_password(password);
    request.set_new_account(new_account);
    request.set_version(version);
    return Ask(request);
  }

  // With the session, a new character in Ember, named after the account
  // and the number.
  nanommo::protocol::LoginReply Create(const std::string& session,
                                       const std::string& name) {
    nanommo::protocol::LoginRequest request;
    request.set_session(session);
    request.set_create_character(true);
    request.set_world("Ember");
    request.set_name(name);
    return Ask(request);
  }

  std::unique_ptr<nanommo::Database> database_;
  std::unique_ptr<nanommo::Redis> redis_;
  std::unique_ptr<nanommo::Accounts> accounts_;
  std::unique_ptr<nanommo::Tokens> tokens_;
  std::unique_ptr<nanommo::Characters> characters_;
  const std::string name_ = nanommo::testing::TestName();
};

}  // namespace

TEST_F(LoginTest, ANewAccountGetsASessionAndNoCharacters) {
  const auto reply = LogIn("correct horse", true);

  EXPECT_TRUE(reply.ok());
  EXPECT_TRUE(tokens_->AccountOf(reply.session()).ok());
  EXPECT_TRUE(reply.token().empty());
  EXPECT_EQ(reply.characters_size(), 0);
}

TEST_F(LoginTest, MakesCharactersAndEntersAsOne) {
  const std::string session = LogIn("correct horse", true).session();

  // A name of its own, in Ember, fresh: it gets its kit as it enters.
  const auto made = Create(session, name_.substr(0, 12) + "_one");
  ASSERT_TRUE(made.ok()) << made.error();
  ASSERT_EQ(made.characters_size(), 1);
  const auto& character = made.characters(0);
  EXPECT_EQ(character.name(), name_.substr(0, 12) + "_one");
  EXPECT_EQ(character.world(), "Ember");
  EXPECT_EQ(character.level(), 1u);
  EXPECT_TRUE(character.fresh());

  // Entering as it gives a token for its id, which a world redeems once.
  nanommo::protocol::LoginRequest enter;
  enter.set_session(session);
  enter.set_enter(true);
  enter.set_character(character.id());
  const auto entered = Ask(enter);
  ASSERT_TRUE(entered.ok()) << entered.error();
  EXPECT_EQ(tokens_->Redeem(entered.token()).value_or(0), character.id());

  // Asking to enter as a character it doesn't have, such as one deleted
  // elsewhere, gets no token, and the characters it does have, as they are.
  enter.set_character(character.id() + 1000000);
  const auto gone = Ask(enter);
  EXPECT_FALSE(gone.ok());
  EXPECT_EQ(gone.error(), "That character isn't yours.");
  EXPECT_TRUE(gone.token().empty());
  EXPECT_EQ(gone.characters_size(), 1);

  // Another account can't enter as it.
  enter.set_character(character.id());
  enter.set_session(*tokens_->Open(character.id() + 1000000));
  const auto someone_else = Ask(enter);
  EXPECT_FALSE(someone_else.ok());
  EXPECT_TRUE(someone_else.token().empty());
}

TEST_F(LoginTest, RefusesATakenNameAndTooManyCharacters) {
  const std::string session = LogIn("correct horse", true).session();
  const std::string name = name_.substr(0, 12);
  ASSERT_TRUE(Create(session, name + "_0").ok());
  EXPECT_EQ(Create(session, name + "_0").error(),
            "Someone in Ember is called that already.");
  for (int i = 1; i < nanommo::Characters::kPerWorld; ++i) {
    ASSERT_TRUE(Create(session, name + "_" + std::to_string(i)).ok());
  }
  EXPECT_EQ(Create(session, name + "_9").error(),
            "You have 4 characters in Ember already.");
  EXPECT_EQ(Create(session, "no").error(),
            "Names are 3 to 16 characters long.");

  // Deleted, it makes room for another, and goes from the list.
  nanommo::protocol::LoginRequest gone;
  gone.set_session(session);
  gone.set_delete_character(true);
  gone.set_character(Create(session, name + "_0").characters(0).id());
  const auto after = Ask(gone);
  ASSERT_TRUE(after.ok()) << after.error();
  EXPECT_EQ(after.characters_size(), nanommo::Characters::kPerWorld - 1);
  EXPECT_TRUE(Create(session, name + "_9").ok());
}

TEST_F(LoginTest, ASessionThatRanOutAsksToLogInAgain) {
  nanommo::protocol::LoginRequest request;
  request.set_session("no such session");
  const auto reply = Ask(request);
  EXPECT_FALSE(reply.ok());
  EXPECT_EQ(reply.error(), "You've been away a while. Log in again.");
}

TEST_F(LoginTest, LogsInWithTheRightPasswordOnly) {
  ASSERT_TRUE(LogIn("correct horse", true).ok());
  EXPECT_TRUE(LogIn("correct horse").ok());

  const auto reply = LogIn("battery staple");
  EXPECT_FALSE(reply.ok());
  EXPECT_EQ(reply.error(), "Wrong name or password.");
  EXPECT_TRUE(reply.session().empty());
}

TEST_F(LoginTest, RefusesANameThatIsTaken) {
  ASSERT_TRUE(LogIn("correct horse", true).ok());

  EXPECT_EQ(LogIn("battery staple", true).error(), "That name is taken.");
}

TEST_F(LoginTest, TurnsAwayAClientOfAnotherVersion) {
  ASSERT_TRUE(LogIn("correct horse", true).ok());

  const auto reply = LogIn("correct horse", false, "0.1.0");
  EXPECT_FALSE(reply.ok());
  EXPECT_TRUE(reply.session().empty());
  EXPECT_EQ(reply.error(),
            absl::StrCat("This client is 0.1.0, and the servers are ",
                         nanommo::kVersion, ". Get the ", nanommo::kVersion,
                         " client to play."));
}

TEST_F(LoginTest, TurnsAwayAClientFromBeforeVersions) {
  const auto reply = LogIn("correct horse", true, "");
  EXPECT_FALSE(reply.ok());
  EXPECT_EQ(reply.error(),
            absl::StrCat("This client is too old for the servers, which are ",
                         nanommo::kVersion, ". Get the new one to play."));
}
