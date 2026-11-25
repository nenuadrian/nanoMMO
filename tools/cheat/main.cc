// The server's red-team client: a player without a window who breaks the
// client's rules, one way at a time, and prints what it gets for it. It's how
// we check what the server already stops, and what it doesn't yet. Run it
// beside the servers, with an account of its own, which it makes the first
// time:
//
//   nanommo_cheat <name> <password> <cheat> [<partner>]
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
//   swing: the nearest monster, with the weapon alone, to compare with.
//   cancel: the same, but the weapon put away after each hit, and back two
//     ticks later, to end the swing early and start the next one sooner.
//
// The walks go east and back west, three seconds each way, so that it stays
// near where it started. The trades are with the partner named after the
// cheat, who runs take:
//
//   take: asks the partner to trade, offers nothing, and is ready once they
//     offer something, to compare with.
//   swap: offers its sword from the bag, and once the partner's ready, moves
//     it to another slot and is ready too, to give the sword and keep it.
//   run: offers its sword, and once the partner's ready, is ready too and
//     leaves at once, to keep the sword when it comes back.
//   twice: comes into the world's second instance too, and trades its sword
//     on the first, which it then leaves. On the second, where it still has
//     the sword, it puts the sword in the bag, so that instance saves its bag
//     over the trade, to keep the sword when it comes back. If the second
//     turns it away, it tries again each second, for twenty seconds, and is
//     ready once it's in, or has given up.
//
// The duels are with the partner too, who says yes in their client, or runs
// duel, which asks back:
//
//   duel: asks the partner to duel, and once it's on, picks them and swings,
//     each input saying it drew them as a client does, 100 milliseconds
//     before the newest world, to compare with.
//   stale: the same, but saying it drew them a second before, to hit where
//     they were rather than where they are.
//
// The war is with the partner too, a friend in the other guild: each founds
// a guild of its own, and asks the other's to war until they're at war.
//
//   feed: stands still until it's killed, comes back as soon as it can, and
//     walks back to the partner, to be killed again.
//   farm: picks the partner and swings each tick, and the server runs it up
//     to them, to run up its guild's score. It prints each kill, and the
//     war's score each time it moves.
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "asio/ssl/context.hpp"
#include "client/connection.h"
#include "protocol/login_client.h"
#include "protocol/nanommo.pb.h"
#include "protocol/settings.h"
#include "protocol/tls.h"
#include "protocol/version.h"
#include "server/history.h"
#include "tools/cheat/measure.h"

namespace {

constexpr uint16_t kLoginPort = 7778;
constexpr uint16_t kWorldPort = 7777;
// The world's second instance, which takes its players over in a deploy.
constexpr uint16_t kNextPort = 7877;
// How long twice tries the second instance for: twice a lease's ten seconds.
constexpr auto kKnocking = std::chrono::seconds(20);
constexpr char kWorld[] = "Ember";
// The server's tick, on which the client sends its inputs.
constexpr auto kTick = std::chrono::milliseconds(50);
constexpr auto kEachWay = std::chrono::seconds(3);
// How long before the newest world a client draws everyone else, which duel
// says it drew them at, and how long before it stale says.
constexpr auto kDrawn = std::chrono::milliseconds(100);
constexpr auto kStale = std::chrono::milliseconds(1000);
// The guilds of farm and feed, at war with each other.
constexpr char kFarmers[] = "Farmers";
constexpr char kFeeders[] = "Feeders";

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

// The player the world has with the id, if it has them.
const nanommo::protocol::Player* Player(const nanommo::protocol::World& world,
                                        uint32_t id) {
  for (const nanommo::protocol::Player& player : world.players()) {
    if (player.id() == id) {
      return &player;
    }
  }
  return nullptr;
}

// Where the world has us, once it has.
const nanommo::protocol::Player* Us(const nanommo::protocol::World& world) {
  return Player(world, world.you());
}

// The player the world has with the name, once it has their name.
const nanommo::protocol::Player* Named(const nanommo::protocol::World& world,
                                       const std::string& name) {
  for (const nanommo::protocol::Player& player : world.players()) {
    if (player.name() == name) {
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

enum class Cheat {
  kWalk,
  kSpeed,
  kTeleport,
  kReach,
  kFight,
  kRapid,
  kSwing,
  kCancel,
  kTake,
  kSwap,
  kRun,
  kTwice,
  kDuel,
  kStale,
  kFeed,
  kFarm
};

// An input that moves what's in one place to another.
nanommo::protocol::Input Move(const nanommo::protocol::Place& from,
                              const nanommo::protocol::Place& to) {
  nanommo::protocol::Input input;
  nanommo::protocol::ItemMove* move = input.mutable_item()->mutable_move();
  *move->mutable_from() = from;
  *move->mutable_to() = to;
  return input;
}

// The first slot of the bag with nothing in it.
uint32_t FreeSlot(const nanommo::protocol::Inventory& inventory) {
  uint32_t slot = 0;
  while (inventory.bag().contains(slot)) {
    ++slot;
  }
  return slot;
}

// An input that does something about a trade.
nanommo::protocol::Input Trading(const nanommo::protocol::TradeAction& action) {
  nanommo::protocol::Input input;
  *input.mutable_trade() = action;
  return input;
}

// How many of the item the player has, in the bag and worn, said in words.
std::string Count(const nanommo::protocol::Inventory& inventory,
                  uint32_t item) {
  int count = 0;
  for (const auto& [slot, stack] : inventory.bag()) {
    count += stack.item() == item ? 1 : 0;
  }
  for (const auto& [slot, stack] : inventory.worn()) {
    count += stack.item() == item ? 1 : 0;
  }
  return absl::StrCat(count, count == 1 ? " sword" : " swords");
}

// The bag's slot that holds the item, if one does.
std::optional<uint32_t> SlotOf(const nanommo::protocol::Inventory& inventory,
                               uint32_t item) {
  for (const auto& [slot, stack] : inventory.bag()) {
    if (stack.item() == item) {
      return slot;
    }
  }
  return std::nullopt;
}

// Comes back into the world's first instance after a second, and prints how
// many swords the player has then. Returns what main does.
int Back(asio::ssl::context& tls, const std::string& name,
         const std::string& password, uint32_t sword) {
  std::this_thread::sleep_for(std::chrono::seconds(1));
  absl::StatusOr<std::string> token = Enter(tls, name, password);
  if (!token.ok()) {
    std::cerr << "cheat: " << name << " couldn't come back: " << token.status()
              << "\n";
    return 1;
  }
  absl::StatusOr<std::unique_ptr<nanommo::Connection>> back =
      nanommo::Connection::Create("localhost", kWorldPort, tls, *token, false);
  if (!back.ok()) {
    std::cerr << "cheat: " << back.status() << "\n";
    return 1;
  }
  while ((*back)->connected() && (*back)->inventory().bag().empty()) {
    (*back)->Poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  std::cout << "cheat: " << name << " is back, and has "
            << Count((*back)->inventory(), sword) << std::endl;
  return 0;
}

// The player in the world's second instance as well, once it has sent them
// their bag, or nothing if it turned them away.
std::unique_ptr<nanommo::Connection> Second(asio::ssl::context& tls,
                                            const std::string& name,
                                            const std::string& password) {
  absl::StatusOr<std::string> token = Enter(tls, name, password);
  if (!token.ok()) {
    return nullptr;
  }
  absl::StatusOr<std::unique_ptr<nanommo::Connection>> second =
      nanommo::Connection::Create("localhost", kNextPort, tls, *token, false);
  if (!second.ok()) {
    return nullptr;
  }
  const auto start = std::chrono::steady_clock::now();
  while ((*second)->connected() && (*second)->inventory().worn().empty() &&
         std::chrono::steady_clock::now() - start < std::chrono::seconds(3)) {
    (*second)->Poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  if (!(*second)->connected() || (*second)->inventory().worn().empty()) {
    return nullptr;
  }
  return *std::move(second);
}

// The trades, with the partner, who runs take. They print what they do, and
// how many swords they have once the trade's over, or for run, once it's
// back. Returns what main does.
int Trade(asio::ssl::context& tls, const std::string& name,
          const std::string& password, Cheat cheat, const std::string& partner,
          std::unique_ptr<nanommo::Connection> connection) {
  nanommo::protocol::Place hand;
  hand.set_worn(nanommo::items::SLOT_MAIN_HAND);
  // The kit's sword, by its item, once the bag has come.
  uint32_t sword = 0;
  std::chrono::steady_clock::time_point asked;
  bool open = false;
  bool stowed = false;
  std::optional<uint32_t> offered;
  // When swap moved the sword, if it has.
  std::optional<std::chrono::steady_clock::time_point> moved;
  bool ready = false;
  // twice is in the second instance too, with a token of its own, once that
  // instance has sent it its bag. One that turns it away, as the first has
  // its lease, takes it once the lease runs out, as it does if the first
  // stalls, so it tries again each second until then.
  std::unique_ptr<nanommo::Connection> next;
  bool knocking = cheat == Cheat::kTwice;
  const auto started = std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point knocked;
  while (connection->connected()) {
    connection->Poll();
    if (knocking &&
        std::chrono::steady_clock::now() - knocked >= std::chrono::seconds(1)) {
      knocked = std::chrono::steady_clock::now();
      next = Second(tls, name, password);
      if (next != nullptr) {
        knocking = false;
        std::cout << "cheat: " << name << " is in Ember a second time, on "
                  << kNextPort << std::endl;
      } else if (knocked - started >= kKnocking) {
        knocking = false;
        std::cout << "cheat: the second instance turned " << name
                  << " away, as it's playing on the first" << std::endl;
      }
    }
    if (next != nullptr) {
      next->Poll();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const nanommo::protocol::Inventory& bag = connection->inventory();
    const auto in_hand = static_cast<uint32_t>(hand.worn());
    if (sword == 0) {
      if (bag.worn().contains(in_hand)) {
        sword = bag.worn().at(in_hand).item();
        std::cout << "cheat: " << name << " has " << Count(bag, sword)
                  << std::endl;
      }
      continue;
    }
    const nanommo::protocol::Trade& trade = connection->trade();
    if (trade.partner() != partner) {
      if (open) {
        // The bag that comes with the trade's end can come a tick after it.
        const auto end = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - end <
               std::chrono::milliseconds(500)) {
          connection->Poll();
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::cout << "cheat: the trade's over, and " << name << " has "
                  << Count(connection->inventory(), sword) << std::endl;
        if (next == nullptr) {
          return 0;
        }
        // The first instance saves the bag without the sword as we leave it.
        // The second still has the sword in hand, and a move to the bag is a
        // change, which it saves on its next tick.
        connection.reset();
        nanommo::protocol::Place free;
        free.set_bag(FreeSlot(next->inventory()));
        next->Send(Move(hand, free));
        const auto put = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - put <
               std::chrono::milliseconds(500)) {
          next->Poll();
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::cout << "cheat: on " << kNextPort << ", " << name
                  << " still has its sword, and puts it in the bag"
                  << std::endl;
        next.reset();
        return Back(tls, name, password, sword);
      }
      // The partner may not be in yet, so we ask again each second.
      const auto now = std::chrono::steady_clock::now();
      if (now - asked >= std::chrono::seconds(1)) {
        nanommo::protocol::TradeAction ask;
        ask.set_ask(partner);
        connection->Send(Trading(ask));
        asked = now;
      }
      continue;
    }
    open = true;
    if (cheat == Cheat::kTake) {
      if (!ready && trade.theirs().bag_size() > 0) {
        nanommo::protocol::TradeAction yes;
        yes.set_ready(true);
        yes.set_revision(trade.revision());
        connection->Send(Trading(yes));
        ready = true;
        std::cout << "cheat: " << partner << " offers a sword, so " << name
                  << " is ready" << std::endl;
      }
      continue;
    }
    // swap, run and twice offer the sword from the bag, so it goes there
    // first.
    if (!offered) {
      if (!stowed) {
        nanommo::protocol::Place free;
        free.set_bag(FreeSlot(bag));
        connection->Send(Move(hand, free));
        stowed = true;
      } else if (const std::optional<uint32_t> slot = SlotOf(bag, sword)) {
        nanommo::protocol::TradeAction offer;
        offer.mutable_offer()->add_bag(*slot);
        connection->Send(Trading(offer));
        offered = slot;
        std::cout << "cheat: " << name << " offers its sword, from slot "
                  << *slot << std::endl;
      }
      continue;
    }
    // twice is ready once it's in the second instance, or has given up.
    if (ready || !trade.theirs().ready() || knocking) {
      continue;
    }
    nanommo::protocol::TradeAction yes;
    yes.set_ready(true);
    yes.set_revision(trade.revision());
    if (cheat == Cheat::kSwap) {
      // The sword leaves the slot it was offered from, and then we're ready.
      if (!moved) {
        nanommo::protocol::Place from;
        from.set_bag(*offered);
        nanommo::protocol::Place to;
        to.set_bag(FreeSlot(bag));
        connection->Send(Move(from, to));
        moved = std::chrono::steady_clock::now();
        std::cout << "cheat: " << partner << " is ready, so " << name
                  << " moves the sword to slot " << to.bag() << std::endl;
      } else if (!bag.bag().contains(*offered)) {
        connection->Send(Trading(yes));
        ready = true;
        std::cout << "cheat: " << name << " is ready too" << std::endl;
      } else if (std::chrono::steady_clock::now() - *moved >
                 std::chrono::seconds(1)) {
        // The server didn't move it, so the trade goes ahead as offered.
        connection->Send(Trading(yes));
        ready = true;
        std::cout << "cheat: the sword's still in slot " << *offered << ", so "
                  << name << " is ready with it there" << std::endl;
      }
      continue;
    }
    connection->Send(Trading(yes));
    if (cheat == Cheat::kTwice) {
      ready = true;
      std::cout << "cheat: " << partner << " is ready, so " << name
                << " is ready too" << std::endl;
      continue;
    }
    // run: ready, and gone before the server's next tick saves the bags.
    connection.reset();
    std::cout << "cheat: " << partner << " is ready, so " << name
              << " is ready too, and leaves at once" << std::endl;
    return Back(tls, name, password, sword);
  }
  std::cout << "cheat: the server closed the connection" << std::endl;
  return 0;
}

// The duels, with the partner. Once one's on, it picks them, and swings at
// them each tick, each input saying when it drew everyone, on the server's
// clock: kDrawn before the newest world, as a client does, or for stale,
// kStale. It prints each blow that lands, and each second what it says, and
// where the partner was then, by its own history of where they've been, as
// the server keeps one. Returns what main does.
int Duel(Cheat cheat, const std::string& partner,
         std::unique_ptr<nanommo::Connection> connection) {
  const std::string named = cheat == Cheat::kStale ? "stale" : "duel";
  const auto says = cheat == Cheat::kStale ? kStale : kDrawn;
  nanommo::History history;
  uint32_t sequence = 0;
  int landed = 0;
  bool fought = false;
  std::chrono::steady_clock::time_point asked;
  std::chrono::steady_clock::time_point tick;
  std::chrono::steady_clock::time_point second;
  std::cout << std::fixed << std::setprecision(1);
  while (connection->connected()) {
    connection->Poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const auto now = std::chrono::steady_clock::now();
    const nanommo::protocol::World& world = connection->world();
    const nanommo::protocol::Duel& duel = connection->duel();
    const std::vector<nanommo::protocol::Event> events =
        connection->TakeEvents();
    if (duel.state() != nanommo::protocol::DUEL_FIGHTING) {
      if (fought) {
        std::cout << "cheat: the duel's over, and " << landed
                  << (landed == 1 ? " blow" : " blows") << " landed"
                  << std::endl;
        return 0;
      }
      // The partner may not be in yet, so we ask each second, and asking
      // back a partner who asked first is a yes.
      const bool waiting = duel.state() == nanommo::protocol::DUEL_ASKING ||
                           duel.state() == nanommo::protocol::DUEL_COUNTDOWN;
      if (!waiting && now - asked >= std::chrono::seconds(1)) {
        nanommo::protocol::Input ask;
        ask.mutable_duel()->set_ask(partner);
        connection->Send(ask);
        asked = now;
      }
      tick = now;
      continue;
    }
    fought = true;
    const uint32_t them = duel.opponent_id();
    const nanommo::protocol::Player* us = Us(world);
    const nanommo::protocol::Player* other = Player(world, them);
    if (us == nullptr || other == nullptr) {
      continue;
    }
    const auto sent = std::chrono::microseconds(world.sent());
    history.Record(them, sent, {.x = other->x(), .z = other->z()});
    // How far from us they are now, or were, as long ago as that.
    const auto away = [&](std::chrono::milliseconds ago) {
      const nanommo::Place then = *history.At(them, sent - ago);
      return std::hypot(then.x - us->x(), then.z - us->z());
    };
    for (const nanommo::protocol::Event& event : events) {
      if (event.kind() == nanommo::protocol::EVENT_HURT &&
          event.player() == them) {
        ++landed;
        std::cout << "cheat: a blow landed on " << partner << ", for "
                  << event.amount() << ", " << away({}) << " m away"
                  << std::endl;
      }
    }
    if (now >= tick) {
      tick += kTick;
      nanommo::protocol::Input input;
      input.set_target_player(them);
      input.set_attack(true);
      input.set_sequence(++sequence);
      input.set_seen(connection->whole());
      input.set_drawn((sent - says).count());
      connection->Send(input);
    }
    if (now - second >= std::chrono::seconds(1)) {
      second = now;
      std::cout << "cheat: " << named << " says it drew " << partner << " "
                << says.count() << " ms ago, " << away(says)
                << " m away then, and " << away({}) << " m away now"
                << std::endl;
    }
  }
  std::cout << "cheat: the server closed the connection" << std::endl;
  return 0;
}

// The war, with the partner, in the other guild. Each founds a guild of its
// own, and asks the partner's to war each second until they're at war, as
// the second to ask says yes. Then feed stands still, comes back once it's
// killed, as soon as it can, and walks back to the partner, and farm picks
// the partner and swings each tick, printing each kill, and the war's score
// as it moves, a few seconds behind. Returns what main does.
int War(Cheat cheat, const std::string& partner,
        std::unique_ptr<nanommo::Connection> connection) {
  const bool farming = cheat == Cheat::kFarm;
  const std::string ours = farming ? kFarmers : kFeeders;
  const std::string theirs = farming ? kFeeders : kFarmers;
  uint32_t sequence = 0;
  int kills = 0;
  bool dead = false;
  uint32_t score = 0;
  std::chrono::steady_clock::time_point asked;
  std::chrono::steady_clock::time_point tick;
  std::chrono::steady_clock::time_point started;
  std::cout << std::fixed << std::setprecision(1);
  while (connection->connected()) {
    connection->Poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const auto now = std::chrono::steady_clock::now();
    const nanommo::protocol::Guild& guild = connection->guild();
    if (guild.war().enemy() != theirs) {
      if (now - asked >= std::chrono::seconds(1)) {
        nanommo::protocol::Input ask;
        if (guild.name().empty()) {
          ask.mutable_guild()->set_create(ours);
        } else {
          ask.mutable_guild()->set_war(theirs);
        }
        connection->Send(ask);
        asked = now;
      }
      started = now;
      continue;
    }
    const nanommo::protocol::World& world = connection->world();
    const nanommo::protocol::Player* us = Us(world);
    const nanommo::protocol::Player* them = Named(world, partner);
    if (us == nullptr || them == nullptr) {
      continue;
    }
    const double minutes =
        std::chrono::duration<double>(now - started).count() / 60;
    if (!farming) {
      const bool died = us->motion() == nanommo::protocol::MOTION_DEAD;
      if (died && world.respawn() == 0 && now >= tick) {
        nanommo::protocol::Input respawn;
        respawn.set_respawn(true);
        connection->Send(respawn);
        tick = now + std::chrono::seconds(1);
      } else if (!died && dead) {
        nanommo::protocol::Input back;
        back.mutable_walk_to()->set_x(them->x());
        back.mutable_walk_to()->set_z(them->z());
        connection->Send(back);
        std::cout << "cheat: " << partner << " killed " << us->name()
                  << ", who walks back" << std::endl;
      }
      dead = died;
      continue;
    }
    if (now >= tick) {
      tick = now + kTick;
      nanommo::protocol::Input input;
      input.set_target_player(them->id());
      input.set_attack(true);
      input.set_sequence(++sequence);
      input.set_seen(connection->whole());
      connection->Send(input);
    }
    const bool killed = them->motion() == nanommo::protocol::MOTION_DEAD;
    if (killed && !dead) {
      ++kills;
      std::cout << "cheat: farm killed " << partner << ", " << kills
                << (kills == 1 ? " time" : " times") << " in " << minutes
                << " minutes" << std::endl;
    }
    dead = killed;
    if (guild.war().kills() != score) {
      score = guild.war().kills();
      std::cout << "cheat: after " << minutes << " minutes, the war's score is "
                << score << " to " << guild.war().enemy_kills() << ", "
                << score / minutes << " a minute" << std::endl;
    }
  }
  std::cout << "cheat: the server closed the connection" << std::endl;
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string usage =
      "usage: nanommo_cheat <name> <password> "
      "<walk|speed|teleport|reach|fight|rapid|swing|cancel>\n"
      "       nanommo_cheat <name> <password> "
      "<take|swap|run|twice|duel|stale|feed|farm> <partner>\n";
  if (argc != 4 && argc != 5) {
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
  } else if (named == "swing") {
    cheat = Cheat::kSwing;
  } else if (named == "cancel") {
    cheat = Cheat::kCancel;
  } else if (named == "take") {
    cheat = Cheat::kTake;
  } else if (named == "swap") {
    cheat = Cheat::kSwap;
  } else if (named == "run") {
    cheat = Cheat::kRun;
  } else if (named == "twice") {
    cheat = Cheat::kTwice;
  } else if (named == "duel") {
    cheat = Cheat::kDuel;
  } else if (named == "stale") {
    cheat = Cheat::kStale;
  } else if (named == "feed") {
    cheat = Cheat::kFeed;
  } else if (named == "farm") {
    cheat = Cheat::kFarm;
  } else {
    std::cerr << usage;
    return 2;
  }
  const bool trades = cheat == Cheat::kTake || cheat == Cheat::kSwap ||
                      cheat == Cheat::kRun || cheat == Cheat::kTwice;
  const bool duels = cheat == Cheat::kDuel || cheat == Cheat::kStale;
  const bool wars = cheat == Cheat::kFeed || cheat == Cheat::kFarm;
  // A trade, a duel or a war needs a partner, and nothing else does.
  if ((trades || duels || wars) != (argc == 5)) {
    std::cerr << usage;
    return 2;
  }
  const bool fights = cheat == Cheat::kReach || cheat == Cheat::kFight ||
                      cheat == Cheat::kRapid || cheat == Cheat::kSwing ||
                      cheat == Cheat::kCancel;

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
  if (trades) {
    return Trade(*tls, name, password, cheat, argv[4], *std::move(connection));
  }
  if (duels) {
    return Duel(cheat, argv[4], *std::move(connection));
  }
  if (wars) {
    return War(cheat, argv[4], *std::move(connection));
  }

  nanommo::Speedometer speed;
  nanommo::Hits hits;
  uint32_t sequence = 0;
  uint32_t target = 0;
  // Where cancel put its weapon, and when it puts it back.
  std::optional<nanommo::protocol::Place> away;
  std::chrono::steady_clock::time_point back;
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
        // Once the blow has landed, the weapon goes into the bag, and back two
        // ticks later, so that the server sees a tick without it.
        const nanommo::protocol::Inventory& bag = (*connection)->inventory();
        const auto hand = static_cast<uint32_t>(nanommo::items::SLOT_MAIN_HAND);
        if (cheat == Cheat::kCancel && !away && bag.worn().contains(hand)) {
          nanommo::protocol::Place worn;
          worn.set_worn(nanommo::items::SLOT_MAIN_HAND);
          away.emplace();
          away->set_bag(FreeSlot(bag));
          (*connection)->Send(Move(worn, *away));
          back = std::chrono::steady_clock::now() + 2 * kTick;
          ++sent;
        }
      }
    }
    if (away && std::chrono::steady_clock::now() >= back) {
      nanommo::protocol::Place worn;
      worn.set_worn(nanommo::items::SLOT_MAIN_HAND);
      (*connection)->Send(Move(*away, worn));
      away.reset();
      ++sent;
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
      const bool skills = cheat == Cheat::kFight || cheat == Cheat::kRapid;
      if (skills && target != 0 && world.skills_size() > 0) {
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
