#include "client/local_connection.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "client/translations.h"
#include "protocol/clock.h"
#include "protocol/messages.h"
#include "server/monsters.h"
#include "server/npcs.h"
#include "server/shown.h"
#include "server/weather.h"
#include "terrain/lands.h"
#include "terrain/terrain.h"

namespace nanommo {
namespace {

// The server's tick, 20 a second.
constexpr std::chrono::milliseconds kTick{50};

// After a long frame, the world catches up a few ticks at once, and no more.
constexpr int kMostTicksAFrame = 5;

// The NPCs, who don't move, go once a second, as from a server.
constexpr int kFullEvery = 20;

constexpr std::size_t kChatLines = 50;

}  // namespace

LocalConnection::LocalConnection(const Content& content, std::string name)
    : content_(std::make_shared<const Content>(content)),
      world_(kSeed, Terrain(kSeed + kLands[kLand].seed, kLands[kLand].wading),
             content_),
      next_tick_(std::chrono::steady_clock::now()) {
  SpawnMonsters(world_, kLand, content_->monsters);
  SpawnNpcs(world_, kLand, content_->npcs);
  npc_cells_ = CellsOf(world_.npcs());
  world_.Join(kYou, Player{.name = std::move(name)});
  std::cout << "nanommo client: offline, with " << world_.monsters().size()
            << " monsters and " << world_.npcs().size() << " NPCs\n";
  Notify(
      Tr("You're playing offline: the world is yours alone, and nothing is "
         "saved."));
}

void LocalConnection::Poll() {
  const auto now = std::chrono::steady_clock::now();
  for (int ticks = 0; now >= next_tick_ && ticks < kMostTicksAFrame; ++ticks) {
    Tick();
    next_tick_ += kTick;
  }
  // Too far behind, as after the page was hidden: on from now.
  if (now >= next_tick_) {
    next_tick_ = now + kTick;
  }
}

void LocalConnection::Send(const protocol::Input& input) {
  inputs_.Take(world_, kYou, input);
  if (input.respawn()) {
    world_.Respawn(kYou);
  }
  if (input.has_walk_to()) {
    world_.WalkTo(kYou, {input.walk_to().x(), input.walk_to().z()});
  }
  if (input.skill() != 0) {
    world_.UseSkill(kYou, input.skill());
  }
  absl::Status done;
  if (input.has_learn()) {
    done.Update(world_.Learn(kYou, input.learn().npc(), input.learn().skill()));
  }
  if (input.has_take()) {
    done.Update(
        world_.TakeQuest(kYou, input.take().npc(), input.take().quest()));
  }
  if (input.has_turn_in()) {
    done.Update(
        world_.TurnIn(kYou, input.turn_in().npc(), input.turn_in().quest()));
  }
  if (input.enlist() != 0) {
    done.Update(world_.Enlist(kYou, input.enlist()));
  }
  if (input.has_item()) {
    done.Update(DoItem(world_, kYou, input.item()));
  }
  if (input.has_craft()) {
    done.Update(DoCraft(world_, kYou, input.craft()));
  }
  if (!done.ok()) {
    Notify(SentenceOf(done).message);
  }
  if (!input.say().empty()) {
    Say(world_.players().at(kYou).name, input.say());
  }
  if (input.has_talk()) {
    Notify(
        Tr("The stories come from a server's storyteller, and there's none "
           "offline."));
  }
  if (input.has_guild()) {
    Notify(Tr("Guilds are kept by the servers, so there are none offline."));
  }
  if (input.has_trade()) {
    Notify(
        Tr("A trade takes another player, and there's nobody else offline."));
  }
  if (input.has_duel()) {
    Notify(Tr("A duel takes another player, and there's nobody else offline."));
  }
}

std::vector<protocol::Event> LocalConnection::TakeEvents() {
  return std::exchange(events_, {});
}

std::optional<protocol::GuildInvite> LocalConnection::TakeInvite() {
  return std::nullopt;
}

void LocalConnection::Tick() {
  inputs_.Apply([this](uint32_t) { return &world_; });
  world_.Step(kTick);
  for (const Notice& notice : world_.TakeNotices()) {
    Notify(notice.message, notice.channel);
  }
  const Player& you = world_.players().at(kYou);
  // What happens near us, as a world server shows it.
  for (const Event& event : world_.TakeEvents()) {
    const std::optional<protocol::Event> seen = ForPlayers(event);
    if (seen && Near(CellOf(you.x, you.z), CellOf(event.x, event.z))) {
      events_.push_back(*seen);
    }
  }
  // A gate takes a player to another land's world, which only a server has.
  const bool in_gate = std::ranges::any_of(kGates, [&you](const Gate& gate) {
    return gate.land == kLand && InGate(gate, you.x, you.z);
  });
  if (in_gate && !in_gate_) {
    Notify(
        Tr("The gates lead to the servers' other lands, and there are none "
           "offline."));
  }
  in_gate_ = in_gate;
  Show();
  inventory_ = BagOf(you).inventory();
  quests_ = QuestsOf(you).quests();
}

void LocalConnection::Show() {
  const bool full = ticks_ == 0;
  ticks_ = (ticks_ + 1) % kFullEvery;
  protocol::World world;
  for (const auto& [cell, view] :
       Views(world_.players(), world_.monsters(), npc_cells_)) {
    if (std::ranges::find(view.here, kYou) == view.here.end()) {
      continue;
    }
    for (const uint32_t id : view.players) {
      *world.add_players() = ToProtocol(id, world_.players().at(id));
    }
    for (const uint32_t id : view.monsters) {
      *world.add_monsters() = ToProtocol(id, world_.monsters().at(id));
    }
    if (full) {
      for (const uint32_t id : view.npcs) {
        *world.add_npcs() = ToProtocol(id, world_.npcs().at(id));
      }
    }
  }
  world.set_sent(NowMicros());
  // Our own part goes last, as in a server's datagram.
  world.MergeFrom(YouIn(kYou, world_.players().at(kYou),
                        kSeed + kLands[kLand].seed, kLand,
                        inputs_.acked(kYou)));
  // The sky the servers would give the land now.
  *world.mutable_sky() = SkyAt(kLand, UnixMs());
  parts_.Add(world);
  shown_ = parts_.world();
}

void LocalConnection::Notify(const std::string& text,
                             protocol::Channel channel) {
  Say("", text, channel);
}

void LocalConnection::Notify(const protocol::Message& message,
                             protocol::Channel channel) {
  protocol::Chat line;
  *line.mutable_message() = message;
  line.set_channel(channel);
  Add(std::move(line));
}

void LocalConnection::Say(const std::string& name, const std::string& text,
                          protocol::Channel channel) {
  protocol::Chat line;
  line.set_name(name);
  line.set_text(text);
  line.set_channel(channel);
  Add(std::move(line));
}

void LocalConnection::Add(protocol::Chat line) {
  chat_.push_back(std::move(line));
  while (chat_.size() > kChatLines) {
    chat_.pop_front();
  }
}

}  // namespace nanommo
