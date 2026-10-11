#include "server/tokens.h"

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "gtest/gtest.h"
#include "server/test_services.h"

namespace {

class TokensTest : public ::testing::Test {
 protected:
  void SetUp() override {
    redis_ = nanommo::testing::TestRedis();
    if (!redis_) {
      GTEST_SKIP() << "set NANOMMO_REDIS_HOST to test against Redis";
    }
  }

  std::unique_ptr<nanommo::Redis> redis_;
};

}  // namespace

TEST_F(TokensTest, RedeemsATokenOnce) {
  nanommo::Tokens tokens(*redis_);
  const absl::StatusOr<std::string> token = tokens.Issue(7);
  ASSERT_TRUE(token.ok());

  EXPECT_EQ(tokens.Redeem(*token).value_or(0), 7u);
  EXPECT_TRUE(absl::IsNotFound(tokens.Redeem(*token).status()));
}

TEST_F(TokensTest, TokensExpire) {
  const absl::StatusOr<std::string> token = nanommo::Tokens(*redis_).Issue(7);
  ASSERT_TRUE(token.ok());

  const absl::StatusOr<std::string> left =
      redis_->Command({"TTL", "token:" + *token});
  int ttl = 0;
  ASSERT_TRUE(left.ok() && absl::SimpleAtoi(*left, &ttl));
  EXPECT_GT(ttl, 0);
  EXPECT_LE(ttl, nanommo::Tokens::kLifetime.count());
}

TEST_F(TokensTest, ReconnectsAfterLosingRedis) {
  const absl::StatusOr<std::string> id = redis_->Command({"CLIENT", "ID"});
  ASSERT_TRUE(id.ok());
  ASSERT_TRUE(
      redis_->Command({"CLIENT", "KILL", "ID", *id, "SKIPME", "no"}).ok());

  nanommo::Tokens tokens(*redis_);
  EXPECT_TRUE(absl::IsUnavailable(tokens.Issue(7).status()));
  EXPECT_TRUE(tokens.Issue(7).ok());
}
