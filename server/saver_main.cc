#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
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

void Flush(nanommo::Players& players, prometheus::Gauge& duplicates) {
  const auto start = std::chrono::steady_clock::now();
  const absl::StatusOr<int> saved = players.Flush();
  if (!saved.ok()) {
    std::cerr << "nanommo saver: " << saved.status() << "\n";
  } else if (*saved > 0) {
    std::cout << "nanommo saver: saved " << *saved << " players in "
              << absl::FormatDuration(
                     absl::FromChrono(std::chrono::steady_clock::now() - start))
              << std::endl;
  }
  // Two items with one serial are an item and its copy, which the alerts
  // watch for.
  const absl::StatusOr<int> twice = players.Duplicates();
  if (!twice.ok()) {
    std::cerr << "nanommo saver: " << twice.status() << "\n";
    return;
  }
  duplicates.Set(*twice);
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
  prometheus::Gauge& duplicates =
      prometheus::BuildGauge()
          .Name("nanommo_duplicate_items")
          .Help("Serials that more than one item in MySQL has")
          .Register(*registry)
          .Add({});
  prometheus::Exposer exposer("0.0.0.0:" + std::to_string(kMetricsPort));
  exposer.RegisterCollectable(registry);

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
    Flush(players, duplicates);
    timer.expires_at(timer.expiry() + kInterval);
    timer.async_wait(self);
  };
  timer.async_wait(tick);

  asio::signal_set signals(io, SIGINT, SIGTERM);
  signals.async_wait([&io](std::error_code, int) { io.stop(); });
  io.run();
  Flush(players, duplicates);
  nanommo::StopTracing();
  std::cout << "nanommo saver: saved what was left, and stopped" << std::endl;
  return 0;
}
