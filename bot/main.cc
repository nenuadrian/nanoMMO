#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/signal_set.hpp"
#include "asio/ssl/context.hpp"
#include "asio/steady_timer.hpp"
#include "bot/bot.h"
#include "bot/bot_metrics.h"
#include "content/content.h"
#include "prometheus/exposer.h"
#include "protocol/clock.h"
#include "protocol/login_client.h"
#include "protocol/nanommo.pb.h"
#include "protocol/settings.h"
#include "protocol/tls.h"

namespace {

constexpr uint16_t kGamePort = 7777;
constexpr uint16_t kLoginPort = 7778;
constexpr uint16_t kMetricsPort = 9465;

// Sixty frames a second, near enough. Sixteen doesn't divide the tick's
// fifty, so the frames land all over the tick, like a client's would.
constexpr std::chrono::milliseconds kFrame(16);

// The world on kGamePort, which the bots play in.
constexpr char kWorld[] = "Ember";

// The login server's answer, or why there's none: it couldn't be reached, or
// it said no.
absl::StatusOr<nanommo::protocol::LoginReply> Ask(
    const std::string& host, asio::ssl::context& tls,
    const nanommo::protocol::LoginRequest& request) {
  absl::StatusOr<nanommo::protocol::LoginReply> reply =
      nanommo::LogIn(host, kLoginPort, tls, request);
  if (reply.ok() && !reply->ok()) {
    return absl::PermissionDeniedError(reply->error());
  }
  return reply;
}

// Logs bot `number` in, making its account the first time, and its
// character in kWorld, named after it, and returns a token to enter as it.
absl::StatusOr<std::string> LogIn(const std::string& host,
                                  asio::ssl::context& tls, int number) {
  const std::string name = "bot" + std::to_string(number);
  nanommo::protocol::LoginRequest request;
  request.set_username(name);
  request.set_password("bot-password");
  absl::StatusOr<nanommo::protocol::LoginReply> reply =
      Ask(host, tls, request);
  if (absl::IsPermissionDenied(reply.status())) {
    request.set_new_account(true);
    reply = Ask(host, tls, request);
  }
  if (!reply.ok()) {
    return reply.status();
  }
  nanommo::protocol::LoginRequest next;
  next.set_session(reply->session());
  const auto mine = [](const nanommo::protocol::LoginReply& listed) {
    const auto found = std::ranges::find_if(
        listed.characters(),
        [](const nanommo::protocol::CharacterListing& character) {
          return character.world() == kWorld;
        });
    return found != listed.characters().end() ? found->id() : 0u;
  };
  uint32_t character = mine(*reply);
  if (character == 0) {
    nanommo::protocol::LoginRequest create = next;
    create.set_create_character(true);
    create.set_world(kWorld);
    create.set_name(name);
    reply = Ask(host, tls, create);
    if (!reply.ok()) {
      return reply.status();
    }
    character = mine(*reply);
  }
  next.set_enter(true);
  next.set_character(character);
  reply = Ask(host, tls, next);
  if (!reply.ok()) {
    return reply.status();
  }
  return reply->token();
}

class Swarm {
 public:
  Swarm(asio::io_context& io, asio::ssl::context& tls, std::string login,
        std::string host, asio::ip::tcp::resolver::results_type server,
        int count, std::chrono::milliseconds ramp, int first, bool udp,
        nanommo::BotMetrics& metrics, const nanommo::Content& content)
      : io_(io),
        tls_(tls),
        login_(std::move(login)),
        host_(std::move(host)),
        server_(std::move(server)),
        count_(count),
        first_(first),
        udp_(udp),
        gap_(count > 0 ? ramp / count : ramp),
        metrics_(metrics),
        content_(content),
        timer_(io),
        report_(io),
        frame_(io) {
    Add();
    Report();
    frame_.expires_after(kFrame);
    frame_.async_wait([this](std::error_code) { Look(); });
  }

 private:
  void Add() {
    if (static_cast<int>(bots_.size()) >= count_) {
      return;
    }
    const int number = first_ + static_cast<int>(bots_.size());
    auto bot = std::make_unique<nanommo::Bot>(
        io_, tls_, static_cast<uint32_t>(number), udp_, content_);
    const absl::StatusOr<std::string> token = LogIn(login_, tls_, number);
    if (token.ok()) {
      bot->Start(server_, *token);
    } else {
      std::cerr << "nanommo bot: bot" << number
                << " couldn't log in: " << token.status() << "\n";
    }
    bots_.push_back(std::move(bot));
    timer_.expires_after(gap_);
    timer_.async_wait([this](std::error_code error) {
      if (!error) {
        Add();
      }
    });
  }

  // A bot whose world hands its players over goes to the world's next
  // instance, as a client does, with the move's token. Bots play against a
  // single machine, so the bot keeps its host, and takes the move's port.
  void Follow() {
    int moved = 0;
    for (std::size_t i = 0; i < bots_.size(); ++i) {
      const std::optional<nanommo::protocol::Move>& move = bots_[i]->move();
      if (!move) {
        continue;
      }
      std::error_code error;
      const asio::ip::tcp::resolver::results_type next =
          asio::ip::tcp::resolver(io_).resolve(
              host_, std::to_string(move->port()), error);
      if (error) {
        std::cerr << "nanommo bot: couldn't find " << host_ << ": "
                  << error.message() << "\n";
        continue;
      }
      const std::string token = move->token();
      const int number = first_ + static_cast<int>(i);
      // The bot before stays until we stop, as its handlers may be waiting
      // on its sockets.
      bots_[i]->Leave();
      left_.push_back(std::move(bots_[i]));
      bots_[i] = std::make_unique<nanommo::Bot>(
          io_, tls_, static_cast<uint32_t>(number), udp_, content_);
      bots_[i]->Start(next, token);
      ++moved;
    }
    if (moved > 0) {
      std::cout << "nanommo bot: " << moved << " bots moved" << std::endl;
    }
  }

  void Report() {
    int connected = 0;
    for (const auto& bot : bots_) {
      connected += bot->connected() ? 1 : 0;
    }
    metrics_.SetConnected(connected);
    // After the count, as a bot that moves takes a moment to connect again.
    Follow();
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

  void Look() {
    const int64_t now = nanommo::NowMicros();
    for (const auto& bot : bots_) {
      if (!bot->connected()) {
        continue;
      }
      for (const int64_t sent : bot->newest()) {
        if (sent > 0) {
          metrics_.ObserveAge(std::chrono::microseconds(now - sent));
        }
      }
    }
    frame_.expires_at(frame_.expiry() + kFrame);
    frame_.async_wait([this](std::error_code error) {
      if (!error) {
        Look();
      }
    });
  }

  asio::io_context& io_;
  asio::ssl::context& tls_;
  std::string login_;
  std::string host_;
  asio::ip::tcp::resolver::results_type server_;
  int count_;
  int first_;
  bool udp_;
  std::chrono::milliseconds gap_;
  nanommo::BotMetrics& metrics_;
  const nanommo::Content& content_;
  asio::steady_timer timer_;
  asio::steady_timer report_;
  asio::steady_timer frame_;
  std::vector<std::unique_ptr<nanommo::Bot>> bots_;
  std::vector<std::unique_ptr<nanommo::Bot>> left_;
  int reported_ = 0;
};

}  // namespace

int main(int argc, char* argv[]) {
  const std::string login = argc > 1 ? argv[1] : "localhost";
  const std::string host = argc > 2 ? argv[2] : "localhost";
  int count = 100;
  int ramp_seconds = 10;
  int first = 1;
  if ((argc > 3 && !absl::SimpleAtoi(argv[3], &count)) ||
      (argc > 4 && !absl::SimpleAtoi(argv[4], &ramp_seconds)) ||
      (argc > 5 && !absl::SimpleAtoi(argv[5], &first))) {
    std::cerr << "nanommo bot: the number of bots, the ramp in seconds and the "
                 "first bot's number should be numbers\n";
    return 1;
  }
  const auto ramp = std::chrono::seconds(ramp_seconds);
  const std::string transport = argc > 6 ? argv[6] : "udp";

  absl::StatusOr<asio::ssl::context> tls =
      nanommo::ClientTls(nanommo::Setting("NANOMMO_TLS_CA", "tls/ca.crt"));
  if (!tls.ok()) {
    std::cerr << "nanommo bot: " << tls.status() << "\n";
    return 1;
  }

  const absl::StatusOr<nanommo::Content> content =
      nanommo::ReadContent(nanommo::Setting("NANOMMO_CONTENT", "content"));
  if (!content.ok()) {
    std::cerr << "nanommo bot: " << content.status() << "\n";
    return 1;
  }

  nanommo::BotMetrics metrics(transport);
  prometheus::Exposer exposer("0.0.0.0:" + std::to_string(kMetricsPort));
  exposer.RegisterCollectable(metrics.registry());

  asio::io_context io;
  asio::ip::tcp::resolver resolver(io);
  std::error_code error;
  const asio::ip::tcp::resolver::results_type server =
      resolver.resolve(host, std::to_string(kGamePort), error);
  if (error) {
    std::cerr << "nanommo bot: couldn't find " << host << ": "
              << error.message() << "\n";
    return 1;
  }
  std::cout << "nanommo bot: starting " << count << " bots against " << host
            << ", logging in on " << login << ", over " << ramp.count()
            << " s, from bot" << first << ", worlds over " << transport
            << std::endl;
  Swarm swarm(io, *tls, login, host, server, count, ramp, first,
              transport == "udp", metrics, *content);

  asio::signal_set signals(io, SIGINT, SIGTERM);
  signals.async_wait([&io](std::error_code, int) { io.stop(); });
  io.run();
  std::cout << "nanommo bot: stopped" << std::endl;
  return 0;
}
