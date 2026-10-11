#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/numbers.h"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/signal_set.hpp"
#include "asio/steady_timer.hpp"
#include "bot/bot.h"

namespace {

class Swarm {
 public:
  Swarm(asio::io_context& io, asio::ip::tcp::resolver::results_type server,
        int count, std::chrono::milliseconds ramp)
      : io_(io),
        server_(std::move(server)),
        count_(count),
        gap_(count > 0 ? ramp / count : ramp),
        timer_(io),
        report_(io) {
    Add();
    Report();
  }

 private:
  void Add() {
    if (static_cast<int>(bots_.size()) >= count_) {
      return;
    }
    auto bot = std::make_unique<nanommo::Bot>(
        io_, static_cast<uint32_t>(bots_.size() + 1));
    bot->Start(server_);
    bots_.push_back(std::move(bot));
    timer_.expires_after(gap_);
    timer_.async_wait([this](std::error_code error) {
      if (!error) {
        Add();
      }
    });
  }

  void Report() {
    int connected = 0;
    for (const auto& bot : bots_) {
      connected += bot->connected() ? 1 : 0;
    }
    if (connected != reported_) {
      std::cout << "nanommo bot: " << connected << " of " << count_
                << " bots connected" << std::endl;
      reported_ = connected;
    }
    report_.expires_after(std::chrono::seconds(1));
    report_.async_wait([this](std::error_code error) {
      if (!error) {
        Report();
      }
    });
  }

  asio::io_context& io_;
  asio::ip::tcp::resolver::results_type server_;
  int count_;
  std::chrono::milliseconds gap_;
  asio::steady_timer timer_;
  asio::steady_timer report_;
  std::vector<std::unique_ptr<nanommo::Bot>> bots_;
  int reported_ = 0;
};

}  // namespace

int main(int argc, char* argv[]) {
  const std::string host = argc > 1 ? argv[1] : "localhost";
  const std::string port = argc > 2 ? argv[2] : "7777";
  int count = 100;
  int ramp_seconds = 10;
  if ((argc > 3 && !absl::SimpleAtoi(argv[3], &count)) ||
      (argc > 4 && !absl::SimpleAtoi(argv[4], &ramp_seconds))) {
    std::cerr << "nanommo bot: the number of bots and the ramp in seconds "
                 "should be numbers\n";
    return 1;
  }
  const auto ramp = std::chrono::seconds(ramp_seconds);

  asio::io_context io;
  asio::ip::tcp::resolver resolver(io);
  std::error_code error;
  const asio::ip::tcp::resolver::results_type server =
      resolver.resolve(host, port, error);
  if (error) {
    std::cerr << "nanommo bot: couldn't find " << host << ": "
              << error.message() << "\n";
    return 1;
  }
  std::cout << "nanommo bot: starting " << count << " bots against " << host
            << ":" << port << ", over " << ramp.count() << " s" << std::endl;
  Swarm swarm(io, server, count, ramp);

  asio::signal_set signals(io, SIGINT, SIGTERM);
  signals.async_wait([&io](std::error_code, int) { io.stop(); });
  io.run();
  std::cout << "nanommo bot: stopped" << std::endl;
  return 0;
}
