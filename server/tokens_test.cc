#include "server/tokens.h"

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "gtest/gtest.h"
#include "server/test_services.h"
#include "server/tracing.h"

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

TEST_F(TokensTest, CarriesOnTheTraceThatIssuedIt) {
  nanommo::Tokens tokens(*redis_);
  nanommo::StartTracing("test",
                        [](const std::string&) { return absl::OkStatus(); });
  nanommo::SpanContext entering;
  absl::StatusOr<std::string> token;
  {
    const nanommo::Span span =
        nanommo::StartSpan("enter world", nanommo::SpanKind::kServer);
    entering = span.context();
    token = tokens.Issue(7);
  }
  nanommo::StopTracing();
  ASSERT_TRUE(token.ok());

  nanommo::SpanContext issuer;
  EXPECT_EQ(tokens.Redeem(*token, &issuer).value_or(0), 7u);
  // Issuing it was a span of the request's, in its trace.
  EXPECT_EQ(issuer.trace_id(), entering.trace_id());
  EXPECT_NE(issuer.span, entering.span);
  // Without tracing, a token is as it was, and carries on nothing.
  const absl::StatusOr<std::string> untraced = tokens.Issue(8);
  ASSERT_TRUE(untraced.ok());
  EXPECT_EQ(tokens.Redeem(*untraced, &issuer).value_or(0), 8u);
  EXPECT_FALSE(issuer.valid());
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
