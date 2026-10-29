#include <httplib.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "protocol/settings.h"
#include "server/recorder.h"
#include "server/redis.h"

namespace {

// ClickHouse's HTTP interface.
constexpr int kClickHousePort = 8123;

constexpr std::chrono::seconds kRetry(5);

std::atomic<bool> stopping = false;

// Inserts the rows into the events table, in ClickHouse's JSONEachRow format,
// a row of JSON a line.
absl::Status Insert(httplib::Client& clickhouse, std::string_view rows) {
  const httplib::Result result =
      clickhouse.Post("/?query=INSERT%20INTO%20events%20FORMAT%20JSONEachRow",
                      rows.data(), rows.size(), "application/x-ndjson");
  if (!result) {
    return absl::UnavailableError(absl::StrCat(
        "couldn't reach ClickHouse: ", httplib::to_string(result.error())));
  }
  if (result->status != 200) {
    return absl::InternalError(absl::StrCat(
        "ClickHouse answered ", result->status, ": ", result->body));
  }
  return absl::OkStatus();
}

}  // namespace

int main() {
  absl::StatusOr<std::unique_ptr<nanommo::Redis>> redis_or =
      nanommo::Redis::Create(
          nanommo::Setting("NANOMMO_REDIS_HOST", "127.0.0.1"));
  if (!redis_or.ok()) {
    std::cerr << "nanommo recorder: " << redis_or.status() << "\n";
    return 1;
  }
  const std::unique_ptr<nanommo::Redis> redis = *std::move(redis_or);
  httplib::Client clickhouse(
      nanommo::Setting("NANOMMO_CLICKHOUSE_HOST", "127.0.0.1"),
      kClickHousePort);
  clickhouse.set_basic_auth(
      "nanommo", nanommo::Setting("NANOMMO_CLICKHOUSE_PASSWORD", "nanommo"));
  clickhouse.set_default_headers({{"X-ClickHouse-Database", "nanommo"}});
  // Redis keeps what a consumer took and didn't acknowledge by its name, so
  // the name stays the same from one container to the next.
  nanommo::Recorder recorder(*redis,
                             nanommo::Setting("NANOMMO_RECORDER", "recorder"),
                             [&clickhouse](std::string_view rows) {
                               return Insert(clickhouse, rows);
                             });
  std::signal(SIGINT, [](int) { stopping = true; });
  std::signal(SIGTERM, [](int) { stopping = true; });
  std::cout << "nanommo recorder: taking events from Redis to ClickHouse"
            << std::endl;

  while (!stopping) {
    const auto start = std::chrono::steady_clock::now();
    const absl::StatusOr<int> recorded = recorder.Record();
    if (!recorded.ok()) {
      std::cerr << "nanommo recorder: " << recorded.status() << "\n";
      std::this_thread::sleep_for(kRetry);
    } else if (*recorded > 0) {
      std::cout << "nanommo recorder: recorded " << *recorded << " events in "
                << absl::FormatDuration(absl::FromChrono(
                       std::chrono::steady_clock::now() - start))
                << std::endl;
    }
  }
  std::cout << "nanommo recorder: stopped" << std::endl;
  return 0;
}
