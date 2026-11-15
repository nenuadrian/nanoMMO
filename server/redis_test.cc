#include "server/redis.h"

#include <memory>
#include <string>

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
