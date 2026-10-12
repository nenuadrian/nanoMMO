#include <chrono>
#include <csignal>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "asio/io_context.hpp"
#include "asio/signal_set.hpp"
#include "asio/steady_timer.hpp"
#include "protocol/settings.h"
#include "server/database.h"
#include "server/players.h"
#include "server/redis.h"

namespace {

constexpr std::chrono::seconds kInterval(5);

void Flush(nanommo::Players& players) {
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

  asio::io_context io;
  asio::steady_timer timer(io, kInterval);
  std::function<void(std::error_code)> tick = [&](std::error_code error) {
    if (error) {
      return;
    }
    Flush(players);
    timer.expires_at(timer.expiry() + kInterval);
    timer.async_wait(tick);
  };
  timer.async_wait(tick);

  asio::signal_set signals(io, SIGINT, SIGTERM);
  signals.async_wait([&io](std::error_code, int) { io.stop(); });
  io.run();
  Flush(players);
  std::cout << "nanommo saver: saved what was left, and stopped" << std::endl;
  return 0;
}
