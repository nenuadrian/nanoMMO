#include "server/redis.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "server/test_services.h"

namespace {

class RedisTest : public ::testing::Test {
 protected:
  void SetUp() override {
    redis_ = nanommo::testing::TestRedis();
    if (!redis_) {
      GTEST_SKIP() << "set NANOMMO_REDIS_HOST to test against Redis";
    }
  }

  void TearDown() override {
    if (redis_) {
      EXPECT_TRUE(redis_->Command({"DEL", hash_, text_}).ok());
    }
  }

  std::unique_ptr<nanommo::Redis> redis_;
  const std::string hash_ = nanommo::testing::TestName();
  const std::string text_ = nanommo::testing::TestName();
};

}  // namespace

TEST_F(RedisTest, ATransactionRedisCantTakeRunsNoneOfIt) {
  EXPECT_FALSE(
      redis_->Transaction({{"HSET", hash_, "gold", "5"}, {"NOT_A_COMMAND"}})
          .ok());
  EXPECT_TRUE(
      absl::IsNotFound(redis_->Command({"HGET", hash_, "gold"}).status()));
  // Every reply was read, so the next command gets its own.
  EXPECT_EQ(redis_->Command({"PING"}).value_or(""), "PONG");

  EXPECT_TRUE(redis_->Transaction({{"HSET", hash_, "gold", "5"}}).ok());
  EXPECT_EQ(redis_->Command({"HGET", hash_, "gold"}).value_or(""), "5");
}

TEST_F(RedisTest, ATransactionFailsIfACommandInItDoes) {
  // Redis takes an HSET onto a key that holds text, and fails it as it runs
  // it, after running the rest, which it doesn't undo.
  ASSERT_TRUE(redis_->Command({"SET", text_, "words"}).ok());
  EXPECT_FALSE(redis_
                   ->Transaction({{"HSET", hash_, "gold", "5"},
                                  {"HSET", text_, "gold", "5"}})
                   .ok());
  EXPECT_EQ(redis_->Command({"HGET", hash_, "gold"}).value_or(""), "5");
  EXPECT_EQ(redis_->Command({"PING"}).value_or(""), "PONG");
}

TEST_F(RedisTest, ATransactionIfRunsOnlyWhileTheKeysHoldTheirValues) {
  ASSERT_TRUE(redis_->Command({"SET", text_, "mine"}).ok());
  // The key holds another value, so none of it runs, and the key comes back.
  absl::StatusOr<std::vector<std::string>> wrong =
      redis_->TransactionIf({{text_, "yours"}}, {{"HSET", hash_, "gold", "5"}});
  ASSERT_TRUE(wrong.ok()) << wrong.status();
  EXPECT_EQ(*wrong, std::vector<std::string>{text_});
  EXPECT_TRUE(
      absl::IsNotFound(redis_->Command({"HGET", hash_, "gold"}).status()));

  wrong = redis_->TransactionIf(
      {{text_, "mine"}},
      {{"HSET", hash_, "gold", "5"}, {"HSET", hash_, "silver", "2"}});
  ASSERT_TRUE(wrong.ok()) << wrong.status();
  EXPECT_TRUE(wrong->empty());
  EXPECT_EQ(redis_->Command({"HGET", hash_, "gold"}).value_or(""), "5");
  EXPECT_EQ(redis_->Command({"HGET", hash_, "silver"}).value_or(""), "2");
}
