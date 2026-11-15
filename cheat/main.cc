// The server's red-team client: a player without a window who breaks the
// client's rules, one way at a time, and prints what it gets for it. It's how
// we check what the server already stops, and what it doesn't yet. Run it
// beside the servers, with an account of its own, which it makes the first
// time:
//
//   nanommo_cheat <name> <password> <cheat>
//
//   walk   one running input a tick, as the client sends, to compare with
//   speed  three running inputs a tick, to run faster than the server lets us
//
// It runs east and back west, three seconds each way, so that it stays near
// where it started.
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "absl/status/statusor.h"
#include "asio/ssl/context.hpp"
#include "cheat/measure.h"
#include "client/connection.h"
#include "protocol/login_client.h"
#include "protocol/nanommo.pb.h"
#include "protocol/settings.h"
#include "protocol/tls.h"
#include "protocol/version.h"

namespace {

constexpr uint16_t kLoginPort = 7778;
constexpr uint16_t kWorldPort = 7777;
constexpr char kWorld[] = "Ember";
// The server's tick, on which the client sends its inputs.
constexpr auto kTick = std::chrono::milliseconds(50);
constexpr auto kEachWay = std::chrono::seconds(3);

absl::StatusOr<nanommo::protocol::LoginReply> Ask(
    asio::ssl::context& tls, const nanommo::protocol::LoginRequest& request) {
  absl::StatusOr<nanommo::protocol::LoginReply> reply =
      nanommo::LogIn("localhost", kLoginPort, tls, request);
  if (reply.ok() && !reply->ok()) {
    return absl::PermissionDeniedError(reply->error());
  }
  return reply;
}

// As the bots do: the account the first time, and a character in kWorld
// named after it. Returns the token for the world.
absl::StatusOr<std::string> Enter(asio::ssl::context& tls,
                                  const std::string& name,
                                  const std::string& password) {
  nanommo::protocol::LoginRequest request;
  request.set_username(name);
  request.set_password(password);
  request.set_version(nanommo::kVersion);
  absl::StatusOr<nanommo::protocol::LoginReply> reply = Ask(tls, request);
  if (absl::IsPermissionDenied(reply.status())) {
    request.set_new_account(true);
    reply = Ask(tls, request);
  }
  if (!reply.ok()) {
    return reply.status();
  }
  const auto mine = [](const nanommo::protocol::LoginReply& listed) {
    for (const nanommo::protocol::CharacterListing& character :
         listed.characters()) {
      if (character.world() == kWorld) {
        return character.id();
      }
    }
    return uint32_t{0};
  };
  nanommo::protocol::LoginRequest next;
  next.set_session(reply->session());
  uint32_t character = mine(*reply);
  if (character == 0) {
    nanommo::protocol::LoginRequest create = next;
    create.set_create_character(true);
    create.set_world(kWorld);
    create.set_name(name);
    reply = Ask(tls, create);
    if (!reply.ok()) {
      return reply.status();
    }
    character = mine(*reply);
  }
  next.set_enter(true);
  next.set_character(character);
  reply = Ask(tls, next);
  if (!reply.ok()) {
    return reply.status();
  }
  return reply->token();
}

// Where the world has us, once it has.
const nanommo::protocol::Player* Us(const nanommo::protocol::World& world) {
  for (const nanommo::protocol::Player& player : world.players()) {
    if (player.id() == world.you()) {
      return &player;
    }
  }
  return nullptr;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string usage =
      "usage: nanommo_cheat <name> <password> <walk|speed>\n";
  if (argc != 4) {
    std::cerr << usage;
    return 2;
  }
  const std::string name = argv[1];
  const std::string password = argv[2];
  const std::string cheat = argv[3];
  int each_tick = 0;
  if (cheat == "walk") {
    each_tick = 1;
  } else if (cheat == "speed") {
    each_tick = 3;
  } else {
    std::cerr << usage;
    return 2;
  }

  absl::StatusOr<asio::ssl::context> tls =
      nanommo::ClientTls(nanommo::FileSetting("NANOMMO_TLS_CA", "tls/ca.crt"));
  if (!tls.ok()) {
    std::cerr << "cheat: " << tls.status() << "\n";
    return 1;
  }
  // The servers may still be starting.
  absl::StatusOr<std::string> token;
  for (int tries = 0; tries < 60; ++tries) {
    token = Enter(*tls, name, password);
    if (token.ok()) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
  if (!token.ok()) {
    std::cerr << "cheat: " << name << " couldn't enter: " << token.status()
              << "\n";
    return 1;
  }
  absl::StatusOr<std::unique_ptr<nanommo::Connection>> connection =
      nanommo::Connection::Create("localhost", kWorldPort, *tls, *token, false);
  if (!connection.ok()) {
    std::cerr << "cheat: " << connection.status() << "\n";
    return 1;
  }
  std::cout << "cheat: " << name << " is in " << kWorld << ", trying " << cheat
            << std::endl;

  nanommo::Speedometer speed;
  uint32_t sequence = 0;
  int sent = 0;
  int64_t last = 0;
  const auto start = std::chrono::steady_clock::now();
  auto tick = start;
  auto second = start;
  while ((*connection)->connected()) {
    (*connection)->Poll();
    const nanommo::protocol::World& world = (*connection)->world();
    if (world.sent() != last) {
      last = world.sent();
      if (const nanommo::protocol::Player* us = Us(world)) {
        speed.Add(static_cast<double>(world.sent()) / 1e6, us->x(), us->z());
      }
    }
    const auto now = std::chrono::steady_clock::now();
    if (now >= tick) {
      tick += kTick;
      const bool east = (now - start) / kEachWay % 2 == 0;
      for (int i = 0; i < each_tick; ++i) {
        nanommo::protocol::Input input;
        input.set_x(east ? 1.0f : -1.0f);
        input.set_run(true);
        input.set_sequence(++sequence);
        input.set_seen((*connection)->whole());
        (*connection)->Send(input);
        ++sent;
      }
    }
    if (now - second >= std::chrono::seconds(1)) {
      second = now;
      std::cout << "cheat: " << cheat << " sent " << sent
                << " inputs a second, and ran " << std::fixed
                << std::setprecision(1) << speed.MetresASecond() << " m/s"
                << std::endl;
      sent = 0;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  std::cout << "cheat: the server closed the connection" << std::endl;
  return 0;
}
