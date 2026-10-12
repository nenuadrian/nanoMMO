#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "asio/io_context.hpp"
#include "asio/signal_set.hpp"
#include "asio/ssl/context.hpp"
#include "protocol/settings.h"
#include "protocol/tls.h"
#include "server/accounts.h"
#include "server/database.h"
#include "server/login_server.h"
#include "server/redis.h"
#include "server/tokens.h"

int main() {
  constexpr uint16_t kPort = 7778;
  absl::StatusOr<asio::ssl::context> tls = nanommo::ServerTls(
      nanommo::Setting("NANOMMO_TLS_CERTIFICATE", "tls/server.crt"),
      nanommo::Setting("NANOMMO_TLS_KEY", "tls/server.key"));
  if (!tls.ok()) {
    std::cerr << "nanommo login: " << tls.status() << "\n";
    return 1;
  }

  // To MySQL's client, localhost means a socket file, so we go over TCP.
  absl::StatusOr<std::unique_ptr<nanommo::Database>> database_or =
      nanommo::Database::Create(
          nanommo::Setting("NANOMMO_MYSQL_HOST", "127.0.0.1"), "nanommo",
          nanommo::Setting("NANOMMO_MYSQL_PASSWORD", "nanommo"), "nanommo");
  if (!database_or.ok()) {
    std::cerr << "nanommo login: " << database_or.status() << "\n";
    return 1;
  }
  const std::unique_ptr<nanommo::Database> database = *std::move(database_or);
  absl::StatusOr<std::unique_ptr<nanommo::Redis>> redis_or =
      nanommo::Redis::Create(
          nanommo::Setting("NANOMMO_REDIS_HOST", "127.0.0.1"));
  if (!redis_or.ok()) {
    std::cerr << "nanommo login: " << redis_or.status() << "\n";
    return 1;
  }
  const std::unique_ptr<nanommo::Redis> redis = *std::move(redis_or);
  nanommo::Accounts accounts(*database);
  nanommo::Tokens tokens(*redis);

  asio::io_context io;
  absl::StatusOr<std::unique_ptr<nanommo::LoginServer>> server_or =
      nanommo::LoginServer::Create(io, kPort, *tls, accounts, tokens);
  if (!server_or.ok()) {
    std::cerr << "nanommo login: " << server_or.status() << "\n";
    return 1;
  }
  const std::unique_ptr<nanommo::LoginServer> server = *std::move(server_or);
  std::cout << "nanommo login: listening on port " << kPort << std::endl;

  asio::signal_set signals(io, SIGINT, SIGTERM);
  signals.async_wait([&io](std::error_code, int) { io.stop(); });
  io.run();
  std::cout << "nanommo login: stopped" << std::endl;
  return 0;
}
