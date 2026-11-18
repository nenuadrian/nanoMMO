#include "server/redis.h"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "gtest/gtest.h"
#include "server/test_services.h"

using std::chrono::milliseconds;
using std::chrono::steady_clock;

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

TEST(RedisTimeouts, ARedisThatDoesntAnswerTimesOut) {
  const nanommo::testing::SilentListener silent;
  absl::StatusOr<std::unique_ptr<nanommo::Redis>> redis =
      nanommo::Redis::Create("127.0.0.1", silent.port(),
                             {.answer = milliseconds(100)});
  // The kernel takes the connection, as it does for a Redis that's hung.
  ASSERT_TRUE(redis.ok()) << redis.status();

  const steady_clock::time_point start = steady_clock::now();
  const absl::StatusOr<std::string> pong = (*redis)->Command({"PING"});
  const steady_clock::duration took = steady_clock::now() - start;
  EXPECT_TRUE(absl::IsUnavailable(pong.status())) << pong.status();
  EXPECT_TRUE(
      absl::StrContains(pong.status().message(), "didn't answer within 100ms"))
      << pong.status();
  EXPECT_GE(took, milliseconds(100));
  EXPECT_LT(took, milliseconds(1000));
}

TEST_F(RedisTest, ARedisThatAnswersInTimeAnswers) {
  absl::StatusOr<std::unique_ptr<nanommo::Redis>> redis =
      nanommo::Redis::Create(std::getenv("NANOMMO_REDIS_HOST"), 6379,
                             {.answer = milliseconds(100)});
  ASSERT_TRUE(redis.ok()) << redis.status();
  EXPECT_EQ((*redis)->Command({"PING"}).value_or(""), "PONG");
}

TEST_F(RedisTest, AReplyThatComesLateAnswersNoOtherCommand) {
  absl::StatusOr<std::unique_ptr<nanommo::Redis>> redis =
      nanommo::Redis::Create(
          std::getenv("NANOMMO_REDIS_HOST"), 6379,
          {.answer = milliseconds(100), .fail_fast = milliseconds(100)});
  ASSERT_TRUE(redis.ok()) << redis.status();
  // Redis hangs for 300 milliseconds, and then answers what it was asked.
  ASSERT_TRUE(redis_->Command({"CLIENT", "PAUSE", "300", "ALL"}).ok());
  EXPECT_TRUE(
      absl::IsUnavailable((*redis)->Command({"ECHO", "first"}).status()));
  std::this_thread::sleep_for(milliseconds(400));

  // The first's reply came after it gave up, on a connection that's gone.
  EXPECT_EQ((*redis)->Command({"ECHO", "second"}).value_or(""), "second");
}

TEST_F(RedisTest, AReadThatBlocksWaitsAsLongOnTopOfTheTimeout) {
  absl::StatusOr<std::unique_ptr<nanommo::Redis>> redis =
      nanommo::Redis::Create(std::getenv("NANOMMO_REDIS_HOST"), 6379,
                             {.answer = milliseconds(100)});
  ASSERT_TRUE(redis.ok()) << redis.status();
  const steady_clock::time_point start = steady_clock::now();
  const absl::StatusOr<std::vector<nanommo::Redis::Entry>> entries =
      (*redis)->Read({"XREAD", "BLOCK", "300", "STREAMS", text_, "$"});
  ASSERT_TRUE(entries.ok()) << entries.status();
  EXPECT_TRUE(entries->empty());
  EXPECT_GE(steady_clock::now() - start, milliseconds(300));
  EXPECT_EQ((*redis)->Command({"PING"}).value_or(""), "PONG");
}

TEST_F(RedisTest, AReadThatBlocksForEverWaitsPastTheTimeout) {
  absl::StatusOr<std::unique_ptr<nanommo::Redis>> redis =
      nanommo::Redis::Create(std::getenv("NANOMMO_REDIS_HOST"), 6379,
                             {.answer = milliseconds(100)});
  ASSERT_TRUE(redis.ok()) << redis.status();
  // The read waits for what comes after the stream's first entry, which the
  // fixture's connection adds 300 milliseconds in, well past the timeout.
  const absl::StatusOr<std::string> first =
      redis_->Command({"XADD", text_, "*", "row", "first"});
  ASSERT_TRUE(first.ok()) << first.status();
  const steady_clock::time_point start = steady_clock::now();
  std::thread later([this] {
    std::this_thread::sleep_for(milliseconds(300));
    EXPECT_TRUE(redis_->Command({"XADD", text_, "*", "row", "later"}).ok());
  });
  const absl::StatusOr<std::vector<nanommo::Redis::Entry>> entries =
      (*redis)->Read({"XREAD", "BLOCK", "0", "STREAMS", text_, *first});
  later.join();
  ASSERT_TRUE(entries.ok()) << entries.status();
  ASSERT_EQ(entries->size(), 1u);
  EXPECT_EQ(entries->front().fields,
            (std::vector<std::string>{"row", "later"}));
  EXPECT_GE(steady_clock::now() - start, milliseconds(300));
}

TEST(RedisTimeouts, FailsFastAfterATimeoutAndAsksAgainAfter) {
  const nanommo::testing::SilentListener silent;
  absl::StatusOr<std::unique_ptr<nanommo::Redis>> redis =
      nanommo::Redis::Create(
          "127.0.0.1", silent.port(),
          {.answer = milliseconds(100), .fail_fast = milliseconds(300)});
  ASSERT_TRUE(redis.ok()) << redis.status();
  ASSERT_FALSE((*redis)->Command({"PING"}).ok());

  // It isn't asked again until the 300 milliseconds are over.
  steady_clock::time_point start = steady_clock::now();
  const absl::StatusOr<std::string> resting = (*redis)->Command({"PING"});
  EXPECT_LT(steady_clock::now() - start, milliseconds(50));
  EXPECT_TRUE(absl::IsUnavailable(resting.status()));
  EXPECT_TRUE(absl::StrContains(resting.status().message(), "isn't asked yet"))
      << resting.status();

  std::this_thread::sleep_for(milliseconds(300));
  start = steady_clock::now();
  EXPECT_FALSE((*redis)->Command({"PING"}).ok());
  EXPECT_GE(steady_clock::now() - start, milliseconds(100));
}
