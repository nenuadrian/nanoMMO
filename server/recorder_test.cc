#include "server/recorder.h"

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "server/events.h"
#include "server/test_services.h"

namespace {

class RecorderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    redis_ = nanommo::testing::TestRedis();
    if (!redis_) {
      GTEST_SKIP() << "set NANOMMO_REDIS_HOST to test against Redis";
    }
    // A stream of our own, from nothing.
    ASSERT_TRUE(redis_->Command({"DEL", nanommo::kEventStream}).ok());
  }

  // A recorder that keeps what it stores, or fails while fail is set.
  nanommo::Recorder Recorder(std::vector<std::string>& stored,
                             const bool& fail) {
    return nanommo::Recorder(
        *redis_, "test",
        [&stored, &fail](std::string_view rows) {
          if (fail) {
            return absl::UnavailableError("ClickHouse is down");
          }
          stored.emplace_back(rows);
          return absl::OkStatus();
        },
        std::chrono::milliseconds(10));
  }

  std::unique_ptr<nanommo::Redis> redis_;
};

}  // namespace

TEST_F(RecorderTest, StoresTheEventsABatchAtATime) {
  std::vector<std::string> stored;
  const bool fail = false;
  nanommo::Recorder recorder = Recorder(stored, fail);
  // The group comes from the first batch, which finds nothing.
  ASSERT_EQ(recorder.Record().value_or(-1), 0);
  nanommo::Events events(*redis_);
  ASSERT_TRUE(events.Add({R"({"kind":"kill"})", R"({"kind":"loot"})"}).ok());

  ASSERT_EQ(recorder.Record().value_or(-1), 2);
  EXPECT_EQ(stored, std::vector<std::string>{
                        "{\"kind\":\"kill\"}\n{\"kind\":\"loot\"}\n"});
  // Each event goes once.
  EXPECT_EQ(recorder.Record().value_or(-1), 0);
  EXPECT_EQ(stored.size(), 1u);
}

TEST_F(RecorderTest, KeepsWhatItCouldntStoreForTheNextTry) {
  std::vector<std::string> stored;
  bool fail = false;
  nanommo::Recorder recorder = Recorder(stored, fail);
  ASSERT_EQ(recorder.Record().value_or(-1), 0);
  ASSERT_TRUE(nanommo::Events(*redis_).Add({R"({"kind":"kill"})"}).ok());

  fail = true;
  EXPECT_FALSE(recorder.Record().ok());
  fail = false;
  // The one it took first comes back, though nobody's added anything since.
  ASSERT_EQ(recorder.Record().value_or(-1), 1);
  EXPECT_EQ(stored, std::vector<std::string>{"{\"kind\":\"kill\"}\n"});
}
