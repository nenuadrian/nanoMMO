#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/time/time.h"
#include "asio/io_context.hpp"
#include "asio/signal_set.hpp"
#include "server/server.h"
#include "server/tick.h"

int main(int argc, char* argv[]) {
  constexpr int kTicksPerSecond = 20;
  const absl::StatusOr<std::chrono::nanoseconds> interval =
      nanommo::TickInterval(kTicksPerSecond);
  if (!interval.ok()) {
    std::cerr << "nanommo server: " << interval.status() << "\n";
    return 1;
  }
  int port = 7777;
  if (argc > 1 &&
      (!absl::SimpleAtoi(argv[1], &port) || port < 1 || port > 65535)) {
    std::cerr
        << "nanommo server: the port should be a number from 1 to 65535, not "
        << argv[1] << "\n";
    return 1;
  }

  asio::io_context io;
  absl::StatusOr<std::unique_ptr<nanommo::Server>> server_or =
      nanommo::Server::Create(io, static_cast<uint16_t>(port), *interval);
  if (!server_or.ok()) {
    std::cerr << "nanommo server: " << server_or.status() << "\n";
    return 1;
  }
  const std::unique_ptr<nanommo::Server> server = *std::move(server_or);
  std::cout << "nanommo server: listening on port " << port << ", "
            << kTicksPerSecond << " ticks/s, "
            << absl::FormatDuration(absl::FromChrono(*interval)) << " per tick"
            << std::endl;

  // As process one in a container, SIGTERM does nothing unless we handle it.
  asio::signal_set signals(io, SIGINT, SIGTERM);
  signals.async_wait([&io](std::error_code, int) { io.stop(); });
  io.run();
  std::cout << "nanommo server: stopped" << std::endl;
  return 0;
}
