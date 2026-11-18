#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "asio/io_context.hpp"
#include "asio/signal_set.hpp"
#include "asio/steady_timer.hpp"
#include "prometheus/exposer.h"
#include "prometheus/gauge.h"
#include "prometheus/registry.h"
#include "protocol/settings.h"
#include "server/database.h"
#include "server/players.h"
#include "server/redis.h"
#include "server/tracing.h"

namespace {

constexpr std::chrono::seconds kInterval(5);
constexpr uint16_t kMetricsPort = 9466;

// What the saver's metrics say.
struct Gauges {
  prometheus::Gauge& duplicates;
  prometheus::Gauge& flushed;
  prometheus::Gauge& unsaved;
};

void Flush(nanommo::Players& players, const Gauges& gauges) {
  const auto start = std::chrono::steady_clock::now();
  const absl::StatusOr<int> saved = players.Flush();
  if (!saved.ok()) {
    std::cerr << "nanommo saver: " << saved.status() << "\n";
  } else {
    // When MySQL last had everything Redis had as the flush began, which an
    // alert can tell the age of, whether the saver's failing or stuck. One
    // that's gone takes its gauge with it, which up says.
    gauges.flushed.Set(absl::ToDoubleSeconds(absl::Now() - absl::UnixEpoch()));
    if (*saved > 0) {
      std::cout << "nanommo saver: saved " << *saved << " players in "
                << absl::FormatDuration(absl::FromChrono(
                       std::chrono::steady_clock::now() - start))
                << std::endl;
    }
  }
  // Those who changed since, for the next flush.
  const absl::StatusOr<int> waiting = players.Unsaved();
  if (waiting.ok()) {
    gauges.unsaved.Set(*waiting);
  }
  // Two items with one serial are an item and its copy, which the alerts
  // watch for.
  const absl::StatusOr<int> twice = players.Duplicates();
  if (!twice.ok()) {
    std::cerr << "nanommo saver: " << twice.status() << "\n";
    return;
  }
  gauges.duplicates.Set(*twice);
}

}  // namespace

int main() {
  absl::StatusOr<std::unique_ptr<nanommo::Database>> database_or =
      nanommo::Database::Create(
          nanommo::Setting("NANOMMO_MYSQL_HOST", "127.0.0.1"), "nanommo",
          nanommo::Setting("NANOMMO_MYSQL_PASSWORD", "nanommo"), "nanommo");
  if (!database_or.ok()) {
    std::cerr << "nanommo saver: " << database_or.status() << "\n";
    return 1;
  }
  const std::unique_ptr<nanommo::Database> database = *std::move(database_or);
  absl::StatusOr<std::unique_ptr<nanommo::Redis>> redis_or =
      nanommo::Redis::Create(
          nanommo::Setting("NANOMMO_REDIS_HOST", "127.0.0.1"));
  if (!redis_or.ok()) {
    std::cerr << "nanommo saver: " << redis_or.status() << "\n";
    return 1;
  }
  const std::unique_ptr<nanommo::Redis> redis = *std::move(redis_or);
  nanommo::Players players(*database, *redis);
  std::cout << "nanommo saver: saving to MySQL every "
            << absl::FormatDuration(absl::FromChrono(kInterval)) << std::endl;

  auto registry = std::make_shared<prometheus::Registry>();
  const Gauges gauges{
      .duplicates = prometheus::BuildGauge()
                        .Name("nanommo_duplicate_items")
                        .Help("Serials that more than one item in MySQL has")
                        .Register(*registry)
                        .Add({}),
      .flushed = prometheus::BuildGauge()
                     .Name("nanommo_saver_flushed_timestamp_seconds")
                     .Help("When the last flush that saved everyone ended")
                     .Register(*registry)
                     .Add({}),
      .unsaved = prometheus::BuildGauge()
                     .Name("nanommo_unsaved_players")
                     .Help("Players in Redis waiting for the next flush")
                     .Register(*registry)
                     .Add({}),
  };
  prometheus::Exposer exposer("0.0.0.0:" + std::to_string(kMetricsPort));
  exposer.RegisterCollectable(registry);
  // It has as long to make its first good flush as it has for each after,
  // from now.
  gauges.flushed.Set(absl::ToDoubleSeconds(absl::Now() - absl::UnixEpoch()));

  // The trades it saves carry on their traces, to an OpenTelemetry collector,
  // such as Alloy, when there's one.
  const std::string collector = nanommo::Setting("NANOMMO_OTLP_ENDPOINT", "");
  if (!collector.empty()) {
    nanommo::StartTracing("saver", nanommo::PostTo(collector), registry.get());
    std::cout << "nanommo saver: sending spans to " << collector << std::endl;
  }

  asio::io_context io;
  asio::steady_timer timer(io, kInterval);
  // self is the lambda, which waits on the timer again, with its return type
  // named, as async_wait checks it.
  auto tick = [&](this auto self, std::error_code error) -> void {
    if (error) {
      return;
    }
    Flush(players, gauges);
    timer.expires_at(timer.expiry() + kInterval);
    timer.async_wait(self);
  };
  timer.async_wait(tick);

  asio::signal_set signals(io, SIGINT, SIGTERM);
  signals.async_wait([&io](std::error_code, int) { io.stop(); });
  io.run();
  Flush(players, gauges);
  nanommo::StopTracing();
  std::cout << "nanommo saver: saved what was left, and stopped" << std::endl;
  return 0;
}
