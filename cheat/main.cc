// The server's red-team client: a player without a window who breaks the
// client's rules, one way at a time, and prints what it gets for it. It's how
// we check what the server already stops, and what it doesn't yet. Run it
// beside the servers, with an account of its own, which it makes the first
// time:
//
//   nanommo_cheat <name> <password> <cheat>
//
// The cheats, each with what it's after:
//
//   walk: one running input a tick, as the client sends, to compare with.
//   speed: three running inputs a tick, to run faster than running.
//   teleport: a direction a thousand times the client's, and then one that
//     isn't a number, east and then west, to go further than running would.
//   reach: a monster about 20 metres away, picked and swung at, to hit it
//     from where it stands.
//   fight: the nearest monster, and the first skill each time it's ready,
//     to compare with.
//   rapid: the same, but the skill ten times a tick, to use it more often.
//
// The walks go east and back west, three seconds each way, so that it stays
// near where it started.
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
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

// The monster the world has with the id, if it has it.
const nanommo::protocol::Monster* Monster(const nanommo::protocol::World& world,
                                          uint32_t id) {
  for (const nanommo::protocol::Monster& monster : world.monsters()) {
    if (monster.id() == id) {
      return &monster;
    }
  }
  return nullptr;
}

// The live monster nearest to being that far from us, or 0 for none.
uint32_t Pick(const nanommo::protocol::World& world,
              const nanommo::protocol::Player& us, float away) {
  uint32_t picked = 0;
  float best = std::numeric_limits<float>::max();
  for (const nanommo::protocol::Monster& monster : world.monsters()) {
    const float off =
        std::abs(std::hypot(monster.x() - us.x(), monster.z() - us.z()) - away);
    if (monster.health() > 0 && off < best) {
      best = off;
      picked = monster.id();
    }
  }
  return picked;
}

enum class Cheat { kWalk, kSpeed, kTeleport, kReach, kFight, kRapid };

}  // namespace

int main(int argc, char** argv) {
  const std::string usage =
      "usage: nanommo_cheat <name> <password> "
      "<walk|speed|teleport|reach|fight|rapid>\n";
  if (argc != 4) {
    std::cerr << usage;
    return 2;
  }
  const std::string name = argv[1];
  const std::string password = argv[2];
  const std::string named = argv[3];
  Cheat cheat = Cheat::kWalk;
  if (named == "walk") {
    cheat = Cheat::kWalk;
  } else if (named == "speed") {
    cheat = Cheat::kSpeed;
  } else if (named == "teleport") {
    cheat = Cheat::kTeleport;
  } else if (named == "reach") {
    cheat = Cheat::kReach;
  } else if (named == "fight") {
    cheat = Cheat::kFight;
  } else if (named == "rapid") {
    cheat = Cheat::kRapid;
  } else {
    std::cerr << usage;
    return 2;
  }
  const bool fights = cheat == Cheat::kReach || cheat == Cheat::kFight ||
                      cheat == Cheat::kRapid;

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
  std::cout << "cheat: " << name << " is in " << kWorld << ", trying " << named
            << std::endl;

  nanommo::Speedometer speed;
  nanommo::Hits hits;
  uint32_t sequence = 0;
  uint32_t target = 0;
  int sent = 0;
  int64_t last = 0;
  const auto start = std::chrono::steady_clock::now();
  auto tick = start;
  auto second = start;
  while ((*connection)->connected()) {
    (*connection)->Poll();
    const auto now = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(now - start).count();
    const nanommo::protocol::World& world = (*connection)->world();
    const nanommo::protocol::Player* us = Us(world);
    if (world.sent() != last && us != nullptr) {
      last = world.sent();
      speed.Add(static_cast<double>(world.sent()) / 1e6, us->x(), us->z());
    }
    for (const nanommo::protocol::Event& event : (*connection)->TakeEvents()) {
      if (event.kind() == nanommo::protocol::EVENT_HIT &&
          event.player() == world.you()) {
        hits.Add(seconds, event.amount());
      }
    }
    // A fight picks its monster once it sees us, and again when it's gone.
    if (fights && us != nullptr) {
      const nanommo::protocol::Monster* picked = Monster(world, target);
      if (picked == nullptr || picked->health() == 0) {
        target = Pick(world, *us, cheat == Cheat::kReach ? 20.0f : 0.0f);
      }
    }
    const auto ways = (now - start) / kEachWay;
    const bool east = ways % 2 == 0;
    // Teleport sends each way as 1000 for three seconds, and then as not a
    // number for three, which leaves the last keys as they were.
    const bool number = ways % 2 == 0;
    const float far = (ways / 2) % 2 == 0 ? 1000.0f : -1000.0f;
    if (now >= tick) {
      tick += kTick;
      // The keys, numbered, once a tick, or three times for speed.
      const int keys = cheat == Cheat::kSpeed ? 3 : 1;
      for (int i = 0; i < keys; ++i) {
        nanommo::protocol::Input input;
        if (cheat == Cheat::kTeleport) {
          input.set_x(number ? far : std::numeric_limits<float>::quiet_NaN());
          input.set_run(true);
        } else if (fights) {
          input.set_target(target);
          input.set_attack(target != 0);
        } else {
          input.set_x(east ? 1.0f : -1.0f);
          input.set_run(true);
        }
        input.set_sequence(++sequence);
        input.set_seen((*connection)->whole());
        (*connection)->Send(input);
        ++sent;
      }
      // A skill goes on its own, unnumbered, as the client sends one: once
      // it's ready for a fight, and ten times a tick for rapid.
      if (target != 0 && world.skills_size() > 0) {
        const uint32_t skill = world.skills(0);
        const bool ready = !world.cooldowns().contains(skill);
        const int uses = cheat == Cheat::kRapid            ? 10
                         : cheat == Cheat::kFight && ready ? 1
                                                           : 0;
        for (int i = 0; i < uses; ++i) {
          nanommo::protocol::Input use;
          use.set_skill(skill);
          (*connection)->Send(use);
          ++sent;
        }
      }
    }
    if (now - second >= std::chrono::seconds(1)) {
      second = now;
      std::cout << "cheat: " << named << " sent " << sent
                << " inputs a second, and ";
      if (cheat == Cheat::kTeleport) {
        std::cout << "with x "
                  << (number ? (far > 0 ? "1000" : "-1000") : "not a number")
                  << " ran ";
      }
      std::cout << std::fixed << std::setprecision(1);
      if (!fights) {
        std::cout << (cheat == Cheat::kTeleport ? "" : "ran ")
                  << speed.MetresASecond() << " m/s";
      } else {
        const nanommo::protocol::Monster* picked = Monster(world, target);
        if (picked != nullptr && us != nullptr) {
          std::cout << "is "
                    << std::hypot(picked->x() - us->x(), picked->z() - us->z())
                    << " m from its monster, ";
        }
        std::cout << "hits " << hits.PerSecond(seconds) << " times a second, "
                  << hits.DamagePerSecond(seconds) << " damage a second";
      }
      std::cout << std::endl;
      sent = 0;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  std::cout << "cheat: the server closed the connection" << std::endl;
  return 0;
}
