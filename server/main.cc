#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/time/time.h"
#include "asio/io_context.hpp"
#include "asio/post.hpp"
#include "asio/signal_set.hpp"
#include "asio/ssl/context.hpp"
#include "asio/steady_timer.hpp"
#include "asio/thread_pool.hpp"
#include "content/content.h"
#include "prometheus/exposer.h"
#include "protocol/settings.h"
#include "protocol/tls.h"
#include "server/consul.h"
#include "server/database.h"
#include "server/metrics.h"
#include "server/players.h"
#include "server/redis.h"
#include "server/server.h"
#include "server/storyteller.h"
#include "server/tick.h"
#include "server/tokens.h"

namespace {

std::optional<uint16_t> ParsePort(std::string_view text) {
  int port = 0;
  if (!absl::SimpleAtoi(text, &port) || port < 1 || port > 65535) {
    return std::nullopt;
  }
  return static_cast<uint16_t>(port);
}

}  // namespace

int main(int argc, char* argv[]) {
  constexpr int kTicksPerSecond = 20;
  const absl::StatusOr<std::chrono::nanoseconds> interval =
      nanommo::TickInterval(kTicksPerSecond);
  if (!interval.ok()) {
    std::cerr << "nanommo server: " << interval.status() << "\n";
    return 1;
  }
  const std::optional<uint16_t> port =
      argc > 1 ? ParsePort(argv[1]) : uint16_t{7777};
  if (!port) {
    std::cerr
        << "nanommo server: the port should be a number from 1 to 65535, not "
        << argv[1] << "\n";
    return 1;
  }
  const std::string metrics_setting =
      nanommo::Setting("NANOMMO_METRICS_PORT", "9464");
  const std::optional<uint16_t> metrics_port = ParsePort(metrics_setting);
  if (!metrics_port) {
    std::cerr << "nanommo server: NANOMMO_METRICS_PORT should be a number from "
                 "1 to 65535, not "
              << metrics_setting << "\n";
    return 1;
  }

  const std::string name = nanommo::Setting("NANOMMO_WORLD", "Ember");
  if (name.empty() || name.size() > 16 ||
      !std::ranges::all_of(name, absl::ascii_isalnum)) {
    std::cerr << "nanommo server: NANOMMO_WORLD should be up to 16 letters "
                 "and digits, not "
              << name << "\n";
    return 1;
  }
  uint32_t seed = 1;
  const std::string seed_setting = nanommo::Setting("NANOMMO_WORLD_SEED", "1");
  if (!absl::SimpleAtoi(seed_setting, &seed)) {
    std::cerr << "nanommo server: NANOMMO_WORLD_SEED should be a whole number "
                 "from 0 to 4294967295, not "
              << seed_setting << "\n";
    return 1;
  }
  const absl::StatusOr<nanommo::Content> content =
      nanommo::ReadContent(nanommo::Setting("NANOMMO_CONTENT", "content"));
  if (!content.ok()) {
    std::cerr << "nanommo server: " << content.status() << "\n";
    return 1;
  }

  absl::StatusOr<asio::ssl::context> tls = nanommo::ServerTls(
      nanommo::Setting("NANOMMO_TLS_CERTIFICATE", "tls/server.crt"),
      nanommo::Setting("NANOMMO_TLS_KEY", "tls/server.key"));
  if (!tls.ok()) {
    std::cerr << "nanommo server: " << tls.status() << "\n";
    return 1;
  }

  nanommo::Metrics metrics;
  prometheus::Exposer exposer("0.0.0.0:" + std::to_string(*metrics_port));
  exposer.RegisterCollectable(metrics.registry());

  absl::StatusOr<std::unique_ptr<nanommo::Database>> database_or =
      nanommo::Database::Create(
          nanommo::Setting("NANOMMO_MYSQL_HOST", "127.0.0.1"), "nanommo",
          nanommo::Setting("NANOMMO_MYSQL_PASSWORD", "nanommo"), "nanommo");
  if (!database_or.ok()) {
    std::cerr << "nanommo server: " << database_or.status() << "\n";
    return 1;
  }
  const std::unique_ptr<nanommo::Database> database = *std::move(database_or);
  absl::StatusOr<std::unique_ptr<nanommo::Redis>> redis_or =
      nanommo::Redis::Create(
          nanommo::Setting("NANOMMO_REDIS_HOST", "127.0.0.1"));
  if (!redis_or.ok()) {
    std::cerr << "nanommo server: " << redis_or.status() << "\n";
    return 1;
  }
  const std::unique_ptr<nanommo::Redis> redis = *std::move(redis_or);
  nanommo::Tokens tokens(*redis);
  nanommo::Players players(*database, *redis);

  asio::io_context io;
  nanommo::Storyteller storyteller(
      io, nanommo::Setting("NANOMMO_STORYTELLER_HOST", "127.0.0.1"));
  absl::StatusOr<std::unique_ptr<nanommo::Server>> server_or =
      nanommo::Server::Create(io, *port, *tls, *interval, name, seed, *content,
                              metrics, tokens, players, storyteller);
  if (!server_or.ok()) {
    std::cerr << "nanommo server: " << server_or.status() << "\n";
    return 1;
  }
  const std::unique_ptr<nanommo::Server> server = *std::move(server_or);
  std::cout << "nanommo server: " << name << ", listening on port " << *port
            << ", " << kTicksPerSecond << " ticks/s, "
            << absl::FormatDuration(absl::FromChrono(*interval)) << " per tick"
            << ", world seed " << seed << std::endl;
  std::cout << "nanommo server: metrics on port " << *metrics_port
            << ", at /metrics" << std::endl;

  nanommo::Consul consul(nanommo::Setting("NANOMMO_CONSUL_HOST", "127.0.0.1"));
  nanommo::WorldRegistration registration{
      .name = name,
      .public_host = nanommo::Setting("NANOMMO_PUBLIC_HOST", "localhost"),
      .port = *port,
      .metrics_port = *metrics_port,
  };
  // Consul gets its own thread, so that a slow answer can't hold up a tick.
  asio::thread_pool consul_thread(1);
  // A beat still waiting on Consul is enough, so that beats can't pile up.
  std::atomic<bool> waiting = false;
  std::optional<int> listed;
  std::string said;
  constexpr std::chrono::seconds kHeartbeat{2};
  asio::steady_timer heartbeat(io);
  std::function<void(std::error_code)> beat = [&](std::error_code error) {
    if (error) {
      return;
    }
    registration.players = server->players();
    if (!waiting.exchange(true)) {
      asio::post(
          consul_thread, [&consul, &listed, &said, &waiting, registration] {
            absl::Status status = absl::OkStatus();
            if (listed != registration.players) {
              status = consul.Register(registration);
            }
            // Registering again keeps a critical check critical, so each beat
            // passes it.
            if (status.ok()) {
              status = consul.Pass(registration.name);
            }
            if (status.ok()) {
              listed = registration.players;
            } else {
              // Consul forgets the worlds when it restarts, so the next beat
              // registers ours again.
              listed.reset();
            }
            const std::string news =
                status.ok() ? "registered with Consul as " + registration.name
                            : status.ToString();
            // Once when it changes, not with each heartbeat.
            if (news != said) {
              said = news;
              std::cout << "nanommo server: " << news << std::endl;
            }
            waiting = false;
          });
    }
    heartbeat.expires_after(kHeartbeat);
    heartbeat.async_wait(beat);
  };
  beat({});

  // As process one in a container, SIGTERM does nothing unless we handle it.
  asio::signal_set signals(io, SIGINT, SIGTERM);
  signals.async_wait([&io](std::error_code, int) { io.stop(); });
  io.run();
  consul_thread.join();
  const absl::Status deregistered = consul.Deregister(name);
  if (!deregistered.ok()) {
    std::cerr << "nanommo server: " << deregistered << "\n";
  }
  std::cout << "nanommo server: stopped" << std::endl;
  return 0;
}
