#include "server/server.h"

#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "asio/buffer.hpp"
#include "asio/socket_base.hpp"
#include "protocol/chat.h"
#include "protocol/clock.h"
#include "protocol/framing.h"
#include "protocol/nanommo.pb.h"
#include "protocol/parts.h"
#include "server/interest.h"
#include "server/levels.h"
#include "server/listen.h"
#include "server/monsters.h"
#include "server/npcs.h"
#include "server/stories.h"
#include "terrain/lands.h"
#include "terrain/terrain.h"
#include "tracy/Tracy.hpp"

namespace nanommo {

namespace {

// A second's worth of ticks.
constexpr int kFullEvery = 20;

// A player can say five lines at once, and then one every two seconds.
constexpr int kChatBurst = 5;
constexpr std::chrono::seconds kChatRefill{2};

// A story is a hundred tokens of the model's work, where a line of chat costs
// nothing, so a player gets one story every ten seconds.
constexpr int kStoryBurst = 1;
constexpr std::chrono::seconds kStoryRefill{10};

protocol::Motion ToProtocol(Motion motion) {
  switch (motion) {
    case Motion::kWalk:
      return protocol::MOTION_WALK;
    case Motion::kRun:
      return protocol::MOTION_RUN;
    case Motion::kAttack:
      return protocol::MOTION_ATTACK;
    case Motion::kDead:
      return protocol::MOTION_DEAD;
    case Motion::kSkill:
      return protocol::MOTION_SKILL;
    case Motion::kIdle:
      break;
  }
  return protocol::MOTION_IDLE;
}

Progress ProgressOf(const Player& player) {
  // The dead come back in the middle, at full health, so that's what we keep.
  if (player.motion == Motion::kDead) {
    return {.experience = player.experience,
            .health = MaxHealth(LevelFor(player.experience).number),
            .land = player.land,
            .weapon = player.weapon,
            .skills = player.skills};
  }
  return {.x = player.x,
          .z = player.z,
          .experience = player.experience,
          .health = player.health,
          .land = player.land,
          .weapon = player.weapon,
          .skills = player.skills};
}

protocol::Player ToProtocol(uint32_t id, const Player& player) {
  protocol::Player sent;
  sent.set_id(id);
  sent.set_x(player.x);
  sent.set_z(player.z);
  sent.set_name(player.name);
  sent.set_yaw(player.yaw);
  sent.set_motion(ToProtocol(player.motion));
  const int level = LevelFor(player.experience).number;
  sent.set_level(level);
  sent.set_health(player.health);
  sent.set_max_health(MaxHealth(level));
  sent.set_weapon(player.weapon);
  if (player.motion == Motion::kSkill) {
    sent.set_skill(player.skill);
  }
  return sent;
}

protocol::Monster ToProtocol(uint32_t id, const Monster& monster) {
  protocol::Monster sent;
  sent.set_id(id);
  sent.set_kind(monster.kind);
  sent.set_x(monster.x);
  sent.set_z(monster.z);
  sent.set_yaw(monster.yaw);
  sent.set_motion(ToProtocol(monster.motion));
  sent.set_level(monster.level);
  sent.set_health(monster.health);
  sent.set_max_health(monster.max_health);
  return sent;
}

protocol::Npc ToProtocol(uint32_t id, const Npc& npc) {
  protocol::Npc sent;
  sent.set_id(id);
  sent.set_kind(npc.kind);
  sent.set_x(npc.x);
  sent.set_z(npc.z);
  sent.set_yaw(npc.yaw);
  return sent;
}

}  // namespace

absl::StatusOr<std::unique_ptr<Server>> Server::Create(
    asio::io_context& io, uint16_t port, asio::ssl::context& tls,
    std::chrono::nanoseconds interval, std::string name, uint32_t seed,
    const Content& content, Metrics& metrics, Tokens& tokens, Players& players,
    Storyteller& storyteller) {
  std::unique_ptr<Server> server = absl::WrapUnique(
      new Server(io, tls, interval, std::move(name), seed, content, metrics,
                 tokens, players, storyteller));
  const absl::Status started = server->Start(port, content);
  if (!started.ok()) {
    return started;
  }
  return server;
}

Server::Server(asio::io_context& io, asio::ssl::context& tls,
               std::chrono::nanoseconds interval, std::string name,
               uint32_t seed, const Content& content, Metrics& metrics,
               Tokens& tokens, Players& players, Storyteller& storyteller)
    : acceptor_(io),
      tls_(tls),
      udp_(io),
      timer_(io),
      interval_(interval),
      name_(std::move(name)),
      seed_(seed),
      metrics_(metrics),
      tokens_(tokens),
      players_(players),
      storyteller_(storyteller) {
  for (const Land& land : kLands) {
    lands_.emplace_back(std::random_device()(),
                        Terrain(seed + land.seed, land.wading), content.items,
                        content.skills);
  }
}

int Server::players() const {
  int count = 0;
  for (const World& land : lands_) {
    count += static_cast<int>(land.players().size());
  }
  return count;
}

uint16_t Server::port() const {
  std::error_code ignored;
  return acceptor_.local_endpoint(ignored).port();
}

absl::Status Server::Start(uint16_t port, const Content& content) {
  const absl::Status listening = Listen(acceptor_, port);
  if (!listening.ok()) {
    return listening;
  }
  // On port zero, the system gave TCP a free port, and UDP takes that one too.
  const absl::Status bound = Bind(udp_, this->port());
  if (!bound.ok()) {
    return bound;
  }
  // One socket sends all the datagrams, so it gets more room for those on
  // their way out than the two hundred kilobytes Linux gives by default.
  std::error_code ignored;
  udp_.set_option(asio::socket_base::send_buffer_size(4 * 1024 * 1024),
                  ignored);
  for (std::size_t index = 0; index < lands_.size(); ++index) {
    World& land = lands_[index];
    SpawnMonsters(land, static_cast<int>(index), content.monsters);
    SpawnNpcs(land, static_cast<int>(index), content.npcs);
    npc_cells_.push_back(CellsOf(land.npcs()));
  }
  Accept();
  Receive();
  timer_.expires_after(interval_);
  timer_.async_wait([this](std::error_code) { Tick(); });
  return absl::OkStatus();
}

World* Server::LandOf(uint32_t id) {
  for (World& land : lands_) {
    if (land.players().contains(id)) {
      return &land;
    }
  }
  return nullptr;
}

void Server::Accept() {
  acceptor_.async_accept([this](std::error_code error,
                                asio::ip::tcp::socket socket) {
    if (!error) {
      auto id = std::make_shared<uint32_t>(0);

      const auto on_message = [this, id](Session& session,
                                         const std::string& bytes) {
        metrics_.AddMessageReceived();
        if (*id == 0) {
          *id = HandleHello(session, bytes);
          return;
        }
        protocol::Input input;
        World* land = LandOf(*id);
        if (land != nullptr && input.ParseFromString(bytes)) {
          land->SetInput(*id, input.x(), input.z(), input.run(),
                         input.attack());
          if (input.skill() != 0) {
            land->UseSkill(*id, input.skill());
          }
          if (!input.say().empty()) {
            Say(*id, input.say());
          }
          if (input.has_talk()) {
            Talk(*id, input.talk());
          }
          if (input.has_learn()) {
            Learn(*id, input.learn());
          }
        }
      };
      const auto on_close = [this, id] {
        if (*id != 0) {
          Leave(*id);
        }
      };
      std::make_shared<Session>(std::move(socket), tls_, on_message, on_close)
          ->Start();
    }
    Accept();
  });
}

uint32_t Server::HandleHello(Session& session, const std::string& bytes) {
  protocol::Hello hello;
  const absl::StatusOr<uint32_t> id =
      hello.ParseFromString(bytes) ? tokens_.Redeem(hello.token())
                                   : absl::InvalidArgumentError("not a hello");
  if (!id.ok() || LandOf(*id) != nullptr) {
    std::cout << "refused a client: unknown token, or already playing"
              << std::endl;
    session.Close();
    return 0;
  }
  const absl::StatusOr<Player> player = players_.Load(name_, *id);
  if (!player.ok()) {
    std::cerr << "couldn't load player " << *id << ": " << player.status()
              << "\n";
    session.Close();
    return 0;
  }
  lands_[player->land].Join(*id, *player);
  saved_[*id] = ProgressOf(*player);
  sessions_[*id] = session.shared_from_this();
  chat_limits_.emplace(*id, TokenBucket(kChatBurst, kChatRefill));
  story_limits_.emplace(*id, TokenBucket(kStoryBurst, kStoryRefill));
  udp_tokens_[hello.token()] = *id;
  std::cout << player->name << " joined, as player " << *id << std::endl;
  return *id;
}

void Server::Leave(uint32_t id) {
  World* land = LandOf(id);
  std::cout << land->players().at(id).name << " left" << std::endl;
  land->Leave(id);
  saved_.erase(id);
  sessions_.erase(id);
  chat_limits_.erase(id);
  story_limits_.erase(id);
  std::erase_if(udp_tokens_,
                [id](const auto& token) { return token.second == id; });
  udp_clients_.erase(id);
}

void Server::Say(uint32_t id, const std::string& text) {
  if (!chat_limits_.at(id).Take(std::chrono::steady_clock::now())) {
    metrics_.AddChatLine("refused");
    Notify(id, "You're talking too fast, so nobody heard that.");
    return;
  }
  const std::optional<std::string> clean = CleanChat(text);
  if (!clean) {
    metrics_.AddChatLine("refused");
    return;
  }
  metrics_.AddChatLine("said");
  protocol::Chat* line = chat_.add_chat();
  line->set_name(LandOf(id)->players().at(id).name);
  line->set_text(*clean);
  std::cout << line->name() << ": " << line->text() << std::endl;
}

void Server::Talk(uint32_t id, const protocol::Talk& talk) {
  const World& land = *LandOf(id);
  const Player& player = land.players().at(id);
  // Each land counts its NPCs from one, so we look in the player's land.
  const auto found = land.npcs().find(talk.npc());
  if (found == land.npcs().end() || !found->second.talks) {
    metrics_.AddStory("refused");
    return;
  }
  const Npc& npc = found->second;
  if (std::hypot(npc.x - player.x, npc.z - player.z) > World::kTalkReach) {
    metrics_.AddStory("refused");
    Notify(id,
           absl::StrCat("You're too far away to talk to the ", npc.name, "."));
    return;
  }
  const std::optional<std::string> words = CleanChat(talk.words());
  const std::optional<std::string> prompt =
      words ? StoryPrompt(*words) : std::nullopt;
  if (!words || !prompt) {
    metrics_.AddStory("refused");
    return;
  }
  const auto asked = std::chrono::steady_clock::now();
  if (!story_limits_.at(id).Take(asked)) {
    metrics_.AddStory("refused");
    Notify(id, absl::StrCat("The ", npc.name,
                            " needs a moment before the next story."));
    return;
  }
  std::cout << player.name << " asked the " << npc.name
            << " for a story: " << *words << std::endl;
  storyteller_.Tell(*prompt, [this, id, asked, name = npc.name](
                                 const absl::StatusOr<std::string>& story) {
    std::vector<std::string> lines = {"I can't think of a story right now."};
    if (story.ok()) {
      metrics_.AddStory("told");
      metrics_.ObserveStory(std::chrono::steady_clock::now() - asked);
      lines = StoryLines(*story);
    } else {
      metrics_.AddStory("failed");
      std::cerr << "no story for player " << id << ": " << story.status()
                << "\n";
    }
    // The player may have left while the model wrote.
    const auto session = sessions_.find(id);
    if (session == sessions_.end()) {
      return;
    }
    protocol::World told;
    for (const std::string& line : lines) {
      if (const std::optional<std::string> clean = CleanChat(line)) {
        protocol::Chat* said = told.add_chat();
        said->set_name(name);
        said->set_text(*clean);
      }
    }
    if (!told.chat().empty()) {
      session->second->Send(Frame(told));
    }
  });
}

void Server::Learn(uint32_t id, const protocol::Learn& learn) {
  World& land = *LandOf(id);
  const absl::Status learned = land.Learn(id, learn.npc(), learn.skill());
  if (!learned.ok()) {
    Notify(id, std::string(learned.message()));
    return;
  }
  std::cout << land.players().at(id).name << " learned skill " << learn.skill()
            << std::endl;
}

void Server::Notify(uint32_t id, const std::string& text) {
  protocol::World notice;
  notice.add_chat()->set_text(text);
  sessions_.at(id)->Send(Frame(notice));
}

void Server::Receive() {
  udp_.async_receive_from(
      asio::buffer(datagram_), sender_,
      [this](std::error_code error, std::size_t size) {
        protocol::Hello hello;
        if (!error &&
            hello.ParseFromArray(datagram_.data(), static_cast<int>(size))) {
          const auto token = udp_tokens_.find(hello.token());
          if (token != udp_tokens_.end()) {
            udp_clients_[token->second] = sender_;
          }
        }
        Receive();
      });
}

void Server::Tick() {
  ZoneScoped;
  const auto start = std::chrono::steady_clock::now();
  for (World& land : lands_) {
    land.Step(interval_);
  }
  Travel();
  for (std::size_t index = 0; index < lands_.size(); ++index) {
    metrics_.SetPlayers(kLands[index].name, lands_[index].players().size());
  }
  SaveChanged();
  Broadcast();
  SendChat();
  metrics_.ObserveTick(std::chrono::steady_clock::now() - start);
  TracyPlot("players", static_cast<int64_t>(players()));
  FrameMark;
  // From the last expiry, not from now, so that the ticks don't drift.
  timer_.expires_at(timer_.expiry() + interval_);
  timer_.async_wait([this](std::error_code) { Tick(); });
}

void Server::Travel() {
  for (std::size_t from = 0; from < lands_.size(); ++from) {
    const Land& here = kLands[from];
    // Leaving changes the map we go through, so first we note who's going.
    std::vector<uint32_t> travellers;
    for (const auto& [id, player] : lands_[from].players()) {
      if (InGate(here, player.x, player.z)) {
        travellers.push_back(id);
      }
    }
    for (const uint32_t id : travellers) {
      Player player = lands_[from].players().at(id);
      lands_[from].Leave(id);
      const Land& there = kLands[here.to];
      const Place arrival = Arrival(there, player.yaw);
      player.x = arrival.x;
      player.z = arrival.z;
      player.land = here.to;
      std::cout << player.name << " went to the " << there.name << std::endl;
      lands_[here.to].Join(id, std::move(player));
    }
  }
}

void Server::SaveChanged() {
  ZoneScoped;
  const auto start = std::chrono::steady_clock::now();
  std::map<uint32_t, Progress> changed;
  for (const World& land : lands_) {
    for (const auto& [id, player] : land.players()) {
      const Progress progress = ProgressOf(player);
      if (progress != saved_[id]) {
        changed[id] = progress;
      }
    }
  }
  const absl::Status status = players_.Save(name_, changed);
  if (status.ok()) {
    for (const auto& [id, progress] : changed) {
      saved_[id] = progress;
    }
  } else {
    std::cerr << "couldn't save " << changed.size() << " players: " << status
              << "\n";
  }
  metrics_.ObserveSave(std::chrono::steady_clock::now() - start);
}

void Server::Broadcast() {
  ZoneScoped;
  // Names, kinds, levels, weapons and the most health a unit can have change
  // now and then, and NPCs never do, so they go out once a second, and the
  // clients keep them in between.
  const bool full = broadcasts_ == 0;
  broadcasts_ = (broadcasts_ + 1) % kFullEvery;
  for (std::size_t index = 0; index < lands_.size(); ++index) {
    BroadcastLand(index, full);
  }
}

void Server::BroadcastLand(std::size_t index, bool full) {
  const World& land = lands_[index];
  // Each player and monster is serialized once a tick, and what each cell
  // gets is put together from those pieces.
  std::map<uint32_t, std::string> players;
  std::map<uint32_t, std::string> monsters;
  std::map<uint32_t, std::string> npcs;
  {
    ZoneScopedN("serialize");
    for (const auto& [id, player] : land.players()) {
      protocol::Player entry = ToProtocol(id, player);
      if (!full) {
        entry.clear_name();
        entry.clear_level();
        entry.clear_max_health();
        entry.clear_weapon();
      }
      players[id] = Piece(entry);
    }
    for (const auto& [id, monster] : land.monsters()) {
      protocol::Monster entry = ToProtocol(id, monster);
      if (!full) {
        entry.clear_kind();
        entry.clear_level();
        entry.clear_max_health();
      }
      monsters[id] = Piece(entry);
    }
    if (full) {
      for (const auto& [id, npc] : land.npcs()) {
        npcs[id] = Piece(ToProtocol(id, npc));
      }
    }
  }
  const int64_t sent = NowMicros();
  for (const auto& [cell, view] :
       Views(land.players(), land.monsters(), npc_cells_[index])) {
    std::vector<std::string_view> pieces;
    pieces.reserve(view.players.size() + view.monsters.size() +
                   (full ? view.npcs.size() : 0));
    for (const uint32_t id : view.players) {
      pieces.push_back(players.at(id));
    }
    for (const uint32_t id : view.monsters) {
      pieces.push_back(monsters.at(id));
    }
    if (full) {
      for (const uint32_t id : view.npcs) {
        pieces.push_back(npcs.at(id));
      }
    }
    // The players in a cell get the same world, so they share its bytes.
    std::shared_ptr<const std::string> whole;
    std::vector<std::shared_ptr<const std::string>> parts;
    for (const uint32_t id : view.here) {
      const auto session = sessions_.find(id);
      if (session == sessions_.end()) {
        continue;
      }
      const Player& player = land.players().at(id);
      const Level level = LevelFor(player.experience);
      protocol::World you;
      you.set_you(id);
      you.set_experience(level.experience);
      you.set_next_level(level.next);
      for (const auto& [skill, left] : player.cooldowns) {
        (*you.mutable_cooldowns())[skill] = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(left)
                .count());
      }
      you.mutable_skills()->Add(player.skills.begin(), player.skills.end());
      you.set_seed(seed_ + kLands[index].seed);
      you.set_land(static_cast<uint32_t>(index));
      std::string own = you.SerializeAsString();
      const auto udp = udp_clients_.find(id);
      if (udp != udp_clients_.end()) {
        if (parts.empty()) {
          for (std::string& part : Split(pieces, sent)) {
            parts.push_back(
                std::make_shared<const std::string>(std::move(part)));
          }
        }
        // A datagram arrives whole or not at all, so it needs no size in front.
        for (const auto& part : parts) {
          metrics_.AddBytesSent(part->size() + own.size());
          SendTo(udp->second, part, own);
        }
      } else {
        if (!whole) {
          protocol::World stamp;
          stamp.set_sent(sent);
          std::string bytes = stamp.SerializeAsString();
          for (const std::string_view piece : pieces) {
            bytes.append(piece);
          }
          whole = std::make_shared<const std::string>(std::move(bytes));
        }
        metrics_.AddBytesSent(kHeaderSize + whole->size() + own.size());
        session->second->Send(whole, std::move(own));
      }
    }
  }
}

void Server::SendChat() {
  if (chat_.chat().empty()) {
    return;
  }
  // Each line has to arrive, so it goes over TCP, even to a client that gets
  // the world over UDP. Everyone in the world hears it, in both lands.
  const auto bytes =
      std::make_shared<const std::string>(chat_.SerializeAsString());
  for (const auto& [id, session] : sessions_) {
    metrics_.AddBytesSent(kHeaderSize + bytes->size());
    session->Send(bytes, {});
  }
  chat_.Clear();
}

void Server::SendTo(const asio::ip::udp::endpoint& client,
                    std::shared_ptr<const std::string> part, std::string own) {
  auto tail = std::make_shared<const std::string>(std::move(own));
  const std::array buffers{asio::buffer(*part), asio::buffer(*tail)};
  // The handler keeps the bytes alive until the datagram has gone.
  udp_.async_send_to(buffers, client,
                     [part = std::move(part), tail = std::move(tail)](
                         std::error_code, std::size_t) {});
}

}  // namespace nanommo
