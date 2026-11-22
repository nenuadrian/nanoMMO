#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
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
#include "bot/backoff.h"
#include "bot/bot.h"
#include "bot/bot_metrics.h"
#include "content/content.h"
#include "prometheus/exposer.h"
#include "protocol/clock.h"
#include "protocol/login_client.h"
#include "protocol/nanommo.pb.h"
#include "protocol/settings.h"
#include "protocol/tls.h"
#include "protocol/version.h"

namespace {

constexpr uint16_t kGamePort = 7777;
constexpr uint16_t kLoginPort = 7778;
constexpr uint16_t kMetricsPort = 9465;

// Sixty frames a second, near enough. Sixteen doesn't divide the tick's
// fifty, so the frames land all over the tick, like a client's would.
constexpr std::chrono::milliseconds kFrame(16);

// The world on kGamePort, which the bots play in.
constexpr char kWorld[] = "Ember";

// At war, how far either side of where new players come in each side waits,
// and how far apart its bots stand, in metres.
constexpr float kCamp = 8.0f;
constexpr float kSpacing = 1.5f;

// How long a bot that's logged in has to reach its world, and how long one
// that's lost its world takes to notice, before it logs in again.
constexpr std::chrono::seconds kGrace(5);

// The most bots that log in again each second, as each waits for the login
// server, and the swarm's other bots for it.
constexpr int kRejoinsPerSecond = 5;

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
  request.set_version(nanommo::kVersion);
  absl::StatusOr<nanommo::protocol::LoginReply> reply = Ask(host, tls, request);
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

// What came of a login, for the metrics.
const char* Outcome(const absl::Status& status) {
  if (status.ok()) {
    return "ok";
  }
  if (absl::IsPermissionDenied(status)) {
    return "refused";
  }
  return absl::IsDeadlineExceeded(status) ? "timeout" : "failed";
}

class Swarm {
 public:
  Swarm(asio::io_context& io, asio::ssl::context& tls, std::string login,
        std::string host, asio::ip::tcp::resolver::results_type server,
        int count, std::chrono::milliseconds ramp, int first, bool udp,
        bool war, nanommo::BotMetrics& metrics, const nanommo::Content& content)
      : io_(io),
        tls_(tls),
        login_(std::move(login)),
        host_(std::move(host)),
        server_(std::move(server)),
        count_(count),
        first_(first),
        udp_(udp),
        war_(war),
        gap_(count > 0 ? ramp / count : ramp),
        metrics_(metrics),
        content_(content),
        timer_(io),
        report_(io),
        frame_(io),
        random_(static_cast<uint32_t>(first)) {
    Add();
    Report();
    frame_.expires_after(kFrame);
    frame_.async_wait([this](std::error_code) { Look(); });
  }

 private:
  // At war, the bots with odd numbers are Red, and the even ones Blue, and the
  // first of each founds and leads it, and asks the rest of its side in.
  std::optional<nanommo::Side> SideOf(int number) const {
    if (!war_) {
      return std::nullopt;
    }
    const bool red = number % 2 == 1;
    nanommo::Side side{.guild = red ? "Red" : "Blue",
                       .enemy = red ? "Blue" : "Red"};
    // The side has every other bot, the first's half rounding up. Each waits
    // in lines of five, a metre and a half apart, eight metres either side of
    // where new players come in, until they're all in the guild.
    const int place = (number - first_) / 2;
    const int line = place / 5;
    const int in_line = place % 5;
    side.size = (count_ + (place * 2 == number - first_ ? 1 : 0)) / 2;
    side.camp_x =
        (red ? -1.0f : 1.0f) * (kCamp + static_cast<float>(line) * kSpacing);
    side.camp_z = (static_cast<float>(in_line) - 2.0f) * kSpacing;
    if (number - first_ < 2) {
      side.leads = true;
      for (int other = number + 2; other < first_ + count_; other += 2) {
        side.recruits.push_back("bot" + std::to_string(other));
      }
    }
    return side;
  }

  void Add() {
    if (static_cast<int>(bots_.size()) >= count_) {
      return;
    }
    const int number = first_ + static_cast<int>(bots_.size());
    auto bot =
        std::make_unique<nanommo::Bot>(io_, tls_, static_cast<uint32_t>(number),
                                       udp_, content_, SideOf(number));
    const auto start = std::chrono::steady_clock::now();
    const absl::StatusOr<std::string> token = LogIn(login_, tls_, number);
    const auto now = std::chrono::steady_clock::now();
    metrics_.ObserveLogin(Outcome(token.status()), now - start);
    if (token.ok()) {
      bot->Start(server_, *token);
      retries_.push_back({.next = now + kGrace});
    } else {
      std::cerr << "nanommo bot: bot" << number
                << " couldn't log in: " << token.status() << "\n";
      retries_.push_back(
          {.next = now + nanommo::Backoff(0, random_), .failed = 1});
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
      bots_[i] = std::make_unique<nanommo::Bot>(io_, tls_,
                                                static_cast<uint32_t>(number),
                                                udp_, content_, SideOf(number));
      bots_[i]->Start(next, token);
      ++moved;
    }
    if (moved > 0) {
      std::cout << "nanommo bot: " << moved << " bots moved" << std::endl;
    }
  }

  // A bot that's lost its world, say as a world that stalled past its leases
  // dropped it, or that couldn't log in, logs in again, as a player would,
  // after a wait that doubles with each try that fails, and a few a second at
  // most.
  void Rejoin() {
    const auto now = std::chrono::steady_clock::now();
    int tried = 0;
    int rejoined = 0;
    for (std::size_t i = 0; i < bots_.size() && tried < kRejoinsPerSecond;
         ++i) {
      Retry& retry = retries_[i];
      if (bots_[i]->connected()) {
        retry = {.next = now + kGrace};
        continue;
      }
      if (bots_[i]->move() || now < retry.next) {
        continue;
      }
      ++tried;
      const int number = first_ + static_cast<int>(i);
      const auto start = std::chrono::steady_clock::now();
      const absl::StatusOr<std::string> token = LogIn(login_, tls_, number);
      metrics_.ObserveLogin(Outcome(token.status()),
                            std::chrono::steady_clock::now() - start);
      if (!token.ok()) {
        std::cerr << "nanommo bot: bot" << number
                  << " couldn't log in again: " << token.status() << "\n";
        retry.next = now + nanommo::Backoff(retry.failed++, random_);
        continue;
      }
      // The bot before stays until we stop, as its handlers may be waiting
      // on its sockets.
      bots_[i]->Leave();
      left_.push_back(std::move(bots_[i]));
      bots_[i] = std::make_unique<nanommo::Bot>(io_, tls_,
                                                static_cast<uint32_t>(number),
                                                udp_, content_, SideOf(number));
      bots_[i]->Start(server_, *token);
      retry.next = now + kGrace;
      ++rejoined;
    }
    if (rejoined > 0) {
      std::cout << "nanommo bot: " << rejoined << " bots logged in again"
                << std::endl;
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
    Rejoin();
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
  bool war_;
  std::chrono::milliseconds gap_;
  nanommo::BotMetrics& metrics_;
  const nanommo::Content& content_;
  asio::steady_timer timer_;
  asio::steady_timer report_;
  asio::steady_timer frame_;
  std::vector<std::unique_ptr<nanommo::Bot>> bots_;
  std::vector<std::unique_ptr<nanommo::Bot>> left_;
  // When each bot, by its place in bots_, may log in again, and how many of
  // its tries have failed in a row.
  struct Retry {
    std::chrono::steady_clock::time_point next;
    int failed = 0;
  };
  std::vector<Retry> retries_;
  std::mt19937 random_;
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
  // war sends the bots to war, in two guilds.
  const bool war = argc > 7 && std::string_view(argv[7]) == "war";

  absl::StatusOr<asio::ssl::context> tls =
      nanommo::ClientTls(nanommo::FileSetting("NANOMMO_TLS_CA", "tls/ca.crt"));
  if (!tls.ok()) {
    std::cerr << "nanommo bot: " << tls.status() << "\n";
    return 1;
  }

  const absl::StatusOr<nanommo::Content> content =
      nanommo::ReadContent(nanommo::FileSetting("NANOMMO_CONTENT", "content"));
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
            << (war ? ", at war" : "") << std::endl;
  Swarm swarm(io, *tls, login, host, server, count, ramp, first,
              transport == "udp", war, metrics, *content);

  asio::signal_set signals(io, SIGINT, SIGTERM);
  signals.async_wait([&io](std::error_code, int) { io.stop(); });
  io.run();
  std::cout << "nanommo bot: stopped" << std::endl;
  return 0;
}
