#include "server/server.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "asio/buffer.hpp"
#include "asio/socket_base.hpp"
#include "protocol/chat.h"
#include "protocol/clock.h"
#include "protocol/framing.h"
#include "protocol/nanommo.pb.h"
#include "protocol/parts.h"
#include "server/consul.h"
#include "server/inputs.h"
#include "server/interest.h"
#include "server/levels.h"
#include "server/listen.h"
#include "server/shown.h"
#include "server/stories.h"
#include "terrain/lands.h"
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

// A drag in the bag is a move, so a player can make a few at once, and then
// ten a second.
constexpr int kItemBurst = 10;
constexpr std::chrono::milliseconds kItemRefill{100};

// A second of the worlds each client was sent, to leave out of the next what
// hasn't changed since the one it has.
constexpr std::size_t kMostRemembered = 20;

// Each guild action is a transaction in MySQL, which the tick waits for, so a
// player gets three at once, and then one a second.
constexpr int kGuildBurst = 3;
constexpr std::chrono::seconds kGuildRefill{1};

// A lease lasts ten seconds, so renewing each second keeps it through a few
// renewals that fail.
constexpr std::chrono::seconds kRenewEvery{1};

Progress ProgressOf(const Player& player) {
  Progress progress{.x = player.x,
                    .z = player.z,
                    .experience = player.experience,
                    .health = player.health,
                    .land = player.land,
                    .weapon = MainHandOf(player),
                    .skills = player.skills,
                    .items_revision = player.inventory.revision(),
                    .quests = player.quests,
                    .quests_done = player.quests_done};
  // The dead come back in the middle, at full health, so that's what we keep.
  if (player.motion == Motion::kDead) {
    progress.x = 0.0f;
    progress.z = 0.0f;
    progress.health = MaxHealthOf(player);
  }
  // A dungeon's copy may be gone by the time they're back, and after a deploy
  // it is, as it ran in the instance before, so we keep them outside it.
  if (kLands[player.land].dungeon) {
    const Gate& out = WayOut(player.land);
    const Place outside = Arrival(out, 0.0f);
    progress.x = outside.x;
    progress.z = outside.z;
    progress.land = out.to;
  }
  return progress;
}

}  // namespace

absl::StatusOr<std::unique_ptr<Server>> Server::Create(
    asio::io_context& io, uint16_t port, asio::ssl::context& tls,
    std::chrono::nanoseconds interval, std::string name, uint32_t seed,
    const Content& content, Metrics& metrics, Tokens& tokens, Players& players,
    Guilds& guilds, Events& events, Storyteller& storyteller) {
  std::unique_ptr<Server> server = absl::WrapUnique(
      new Server(io, tls, interval, std::move(name), seed, content, metrics,
                 tokens, players, guilds, events, storyteller));
  const absl::Status started = server->Start(port);
  if (!started.ok()) {
    return started;
  }
  return server;
}

Server::Server(asio::io_context& io, asio::ssl::context& tls,
               std::chrono::nanoseconds interval, std::string name,
               uint32_t seed, const Content& content, Metrics& metrics,
               Tokens& tokens, Players& players, Guilds& guilds, Events& events,
               Storyteller& storyteller)
    : acceptor_(io),
      tls_(tls),
      udp_(io),
      timer_(io),
      interval_(interval),
      name_(std::move(name)),
      seed_(seed),
      copies_(seed, content),
      metrics_(metrics),
      tokens_(tokens),
      players_(players),
      guilds_(guilds),
      events_(events),
      storyteller_(storyteller) {}

int Server::players() const { return copies_.players(); }

uint16_t Server::port() const {
  std::error_code ignored;
  return acceptor_.local_endpoint(ignored).port();
}

absl::Status Server::Start(uint16_t port) {
  const absl::Status listening = Listen(acceptor_, port);
  if (!listening.ok()) {
    return listening;
  }
  // On port zero, the system gave TCP a free port, and UDP takes that one too.
  const absl::Status bound = Bind(udp_, this->port());
  if (!bound.ok()) {
    return bound;
  }
  instance_ = InstanceId({.name = name_, .port = this->port()});
  // One socket sends all the datagrams, so it gets more room for those on
  // their way out than the two hundred kilobytes Linux gives by default.
  std::error_code ignored;
  udp_.set_option(asio::socket_base::send_buffer_size(4 * 1024 * 1024),
                  ignored);
  Accept();
  Receive();
  timer_.expires_after(interval_);
  timer_.async_wait([this](std::error_code) { Tick(); });
  return absl::OkStatus();
}

World* Server::WorldOf(uint32_t id) {
  Copy* copy = copies_.Of(id);
  return copy != nullptr ? &copy->world : nullptr;
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
        World* land = WorldOf(*id);
        if (land != nullptr && input.ParseFromString(bytes)) {
          if (input.seen() > seen_[*id]) {
            seen_[*id] = input.seen();
          }
          inputs_.Take(*land, *id, input);
          if (input.respawn()) {
            land->Respawn(*id);
          }
          if (input.has_walk_to()) {
            land->WalkTo(*id, {input.walk_to().x(), input.walk_to().z()});
          }
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
          if (input.has_take()) {
            TakeQuest(*id, input.take());
          }
          if (input.has_turn_in()) {
            TurnIn(*id, input.turn_in());
          }
          if (input.has_item()) {
            Item(*id, input.item());
          }
          if (input.has_guild()) {
            Guild(*id, input.guild());
          }
          if (input.has_trade()) {
            Trade(*id, input.trade());
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
  if (!id.ok() || WorldOf(*id) != nullptr) {
    std::cout << "refused a client: unknown token, or already playing"
              << std::endl;
    session.Close();
    return 0;
  }
  // A character plays on one instance of its world at a time, which has their
  // lease while they do, so that the others turn them away.
  const absl::StatusOr<std::string> lease =
      players_.Lease(name_, *id, instance_);
  if (!lease.ok()) {
    if (absl::IsFailedPrecondition(lease.status())) {
      std::cout << "refused player " << *id << ": " << lease.status().message()
                << std::endl;
    } else {
      std::cerr << "couldn't lease player " << *id << ": " << lease.status()
                << "\n";
    }
    session.Close();
    return 0;
  }
  const absl::StatusOr<Player> player = players_.Load(name_, *id);
  if (!player.ok()) {
    std::cerr << "couldn't load player " << *id << ": " << player.status()
              << "\n";
    players_.Release(name_, *id, *lease).IgnoreError();
    session.Close();
    return 0;
  }
  copies_.Join(*id, *player);
  // Their guild is in MySQL, as others change it too, and they get it, or
  // that they're in none, with the first guild news.
  const absl::StatusOr<Membership> membership = guilds_.Of(*id);
  if (membership.ok()) {
    members_[*id] = *membership;
    // The rest of the guild sees them online.
    RefreshGuild(membership->guild);
  } else if (!absl::IsNotFound(membership.status())) {
    std::cerr << "couldn't load player " << *id
              << "'s guild: " << membership.status() << "\n";
  }
  guild_news_.insert(*id);
  // Names, kinds and NPCs go out once a second, so the next broadcast is a
  // full one, and a player who's just joined, say from another instance,
  // gets them in their first world.
  broadcasts_ = 0;
  saved_[*id] = ProgressOf(*player);
  leases_[*id] = *lease;
  sessions_[*id] = session.shared_from_this();
  chat_limits_.emplace(*id, TokenBucket(kChatBurst, kChatRefill));
  story_limits_.emplace(*id, TokenBucket(kStoryBurst, kStoryRefill));
  item_limits_.emplace(*id, TokenBucket(kItemBurst, kItemRefill));
  guild_limits_.emplace(*id, TokenBucket(kGuildBurst, kGuildRefill));
  udp_tokens_[hello.token()] = *id;
  std::cout << player->name << " joined, as player " << *id << ", with lease "
            << *lease << std::endl;
  return *id;
}

void Server::Leave(uint32_t id) {
  std::cout << WorldOf(id)->players().at(id).name << " left" << std::endl;
  // Nothing saves them once they're gone, so what's changed since the last
  // tick goes now. The next instance has loaded them as they were saved when
  // the handover began, which this mustn't write over.
  if (!handing_over_) {
    Save({id});
  }
  // So that another instance can take them at once, not once it runs out.
  if (const auto lease = leases_.find(id); lease != leases_.end()) {
    const absl::Status released = players_.Release(name_, id, lease->second);
    if (!released.ok()) {
      std::cerr << "couldn't release player " << id << "'s lease: " << released
                << "\n";
    }
    leases_.erase(lease);
  }
  copies_.Leave(id);
  saved_.erase(id);
  sessions_.erase(id);
  chat_limits_.erase(id);
  story_limits_.erase(id);
  item_limits_.erase(id);
  guild_limits_.erase(id);
  inputs_.Leave(id);
  history_.erase(id);
  seen_.erase(id);
  if (const auto member = members_.find(id); member != members_.end()) {
    const uint32_t guild = member->second.guild;
    members_.erase(member);
    RefreshGuild(guild);
  }
  guild_news_.erase(id);
  sent_bags_.erase(id);
  sent_quests_.erase(id);
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
  line->set_name(WorldOf(id)->players().at(id).name);
  line->set_text(*clean);
  std::cout << line->name() << ": " << line->text() << std::endl;
}

void Server::Talk(uint32_t id, const protocol::Talk& talk) {
  const World& land = *WorldOf(id);
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
  World& land = *WorldOf(id);
  const absl::Status learned = land.Learn(id, learn.npc(), learn.skill());
  if (!learned.ok()) {
    Notify(id, std::string(learned.message()));
    return;
  }
  std::cout << land.players().at(id).name << " learned skill " << learn.skill()
            << std::endl;
}

void Server::TakeQuest(uint32_t id, const protocol::Quest& quest) {
  World& land = *WorldOf(id);
  const absl::Status taken = land.TakeQuest(id, quest.npc(), quest.quest());
  if (!taken.ok()) {
    Notify(id, std::string(taken.message()));
    return;
  }
  std::cout << land.players().at(id).name << " took quest " << quest.quest()
            << std::endl;
}

void Server::TurnIn(uint32_t id, const protocol::Quest& quest) {
  World& land = *WorldOf(id);
  const absl::Status turned = land.TurnIn(id, quest.npc(), quest.quest());
  if (!turned.ok()) {
    Notify(id, std::string(turned.message()));
    return;
  }
  std::cout << land.players().at(id).name << " turned in quest "
            << quest.quest() << std::endl;
}

void Server::Item(uint32_t id, const protocol::ItemAction& action) {
  // The next instance loads the bag as it was last saved, so nothing changes
  // it here after that.
  if (handing_over_) {
    return;
  }
  if (!item_limits_.at(id).Take(std::chrono::steady_clock::now())) {
    Notify(id, "You're moving things too fast.");
    return;
  }
  const absl::Status done = DoItem(*WorldOf(id), id, action);
  if (!done.ok()) {
    Notify(id, std::string(done.message()));
  }
}

void Server::Trade(uint32_t id, const protocol::TradeAction& action) {
  // A trade changes two bags, which the next instance loads as they were
  // last saved, so nothing trades here after that.
  if (handing_over_) {
    return;
  }
  // It's moving things too, so it's on the bag's limit.
  if (!item_limits_.at(id).Take(std::chrono::steady_clock::now())) {
    Notify(id, "You're moving things too fast.");
    return;
  }
  World& land = *WorldOf(id);
  absl::Status done;
  switch (action.action_case()) {
    case protocol::TradeAction::kAsk:
      done = land.AskToTrade(id, action.ask());
      break;
    case protocol::TradeAction::kOffer:
      done = land.OfferToTrade(id, action.offer());
      break;
    case protocol::TradeAction::kReady: {
      const absl::StatusOr<uint32_t> traded =
          land.ReadyToTrade(id, action.ready(), action.revision());
      done = traded.status();
      // Both bags go into Redis together, as they trade, before either player
      // hears of it, so that a crash or a leave can't keep one and lose the
      // other.
      if (traded.value_or(0) != 0) {
        Save({id, *traded});
      }
      break;
    }
    case protocol::TradeAction::kCancel:
      land.CancelTrade(id);
      break;
    case protocol::TradeAction::ACTION_NOT_SET:
      return;
  }
  if (!done.ok()) {
    Notify(id, std::string(done.message()));
  }
}

void Server::Guild(uint32_t id, const protocol::GuildAction& action) {
  // Like the bag, so that the next instance reads the guild as it'll stay.
  if (handing_over_) {
    return;
  }
  // A line of chat has the chat's own limit, and doesn't touch MySQL.
  if (action.has_say()) {
    GuildSay(id, action.say());
    return;
  }
  if (!guild_limits_.at(id).Take(std::chrono::steady_clock::now())) {
    Notify(id, "You're doing that too fast.");
    return;
  }
  switch (action.action_case()) {
    case protocol::GuildAction::kCreate:
      Found(id, action.create());
      break;
    case protocol::GuildAction::kInvite:
      Ask(id, action.invite());
      break;
    case protocol::GuildAction::kAccept:
      Join(id);
      break;
    case protocol::GuildAction::kDecline:
      if (const absl::Status declined = guilds_.Decline(id); !declined.ok()) {
        Refuse(id, declined);
      }
      break;
    case protocol::GuildAction::kLeave:
      QuitGuild(id);
      break;
    case protocol::GuildAction::kPromote:
      SetRank(id, action.promote(), protocol::RANK_OFFICER);
      break;
    case protocol::GuildAction::kDemote:
      SetRank(id, action.demote(), protocol::RANK_MEMBER);
      break;
    case protocol::GuildAction::kKick:
      Kick(id, action.kick());
      break;
    case protocol::GuildAction::kDisband:
      Disband(id);
      break;
    case protocol::GuildAction::kSay:
    case protocol::GuildAction::ACTION_NOT_SET:
      break;
  }
}

void Server::Found(uint32_t id, const std::string& name) {
  const absl::StatusOr<Membership> founded = guilds_.Create(name_, id, name);
  if (!founded.ok()) {
    Refuse(id, founded.status());
    return;
  }
  members_[id] = *founded;
  guild_news_.insert(id);
  std::cout << WorldOf(id)->players().at(id).name << " founded "
            << founded->name << std::endl;
}

void Server::Ask(uint32_t id, const std::string& name) {
  // They have to be playing here, to see the question.
  uint32_t asked = 0;
  std::string asked_name;
  for (const auto& [copy_id, copy] : copies_.all()) {
    for (const auto& [other, player] : copy.world.players()) {
      if (absl::EqualsIgnoreCase(player.name, name)) {
        asked = other;
        asked_name = player.name;
      }
    }
  }
  if (asked == 0) {
    Notify(id,
           absl::StrCat("Nobody of that name is playing in ", name_, " now."));
    return;
  }
  if (members_.contains(asked)) {
    Notify(id, absl::StrCat(asked_name, " is in a guild already."));
    return;
  }
  const absl::StatusOr<Membership> guild = guilds_.Invite(id, asked);
  if (!guild.ok()) {
    Refuse(id, guild.status());
    return;
  }
  protocol::World question;
  question.mutable_invite()->set_guild(guild->name);
  question.mutable_invite()->set_from(WorldOf(id)->players().at(id).name);
  sessions_.at(asked)->Send(Frame(question));
  Notify(id,
         absl::StrCat("You asked ", asked_name, " to join ", guild->name, "."));
}

void Server::Join(uint32_t id) {
  const absl::StatusOr<Membership> joined = guilds_.Accept(id);
  if (!joined.ok()) {
    Refuse(id, joined.status());
    return;
  }
  members_[id] = *joined;
  RefreshGuild(joined->guild);
  TellGuild(joined->guild, absl::StrCat(WorldOf(id)->players().at(id).name,
                                        " joined ", joined->name, "."));
}

void Server::QuitGuild(uint32_t id) {
  const absl::StatusOr<Left> left = guilds_.Leave(id);
  if (!left.ok()) {
    Refuse(id, left.status());
    return;
  }
  members_.erase(id);
  guild_news_.insert(id);
  RefreshGuild(left->guild);
  std::string news = absl::StrCat(WorldOf(id)->players().at(id).name, " left ",
                                  left->name, ".");
  if (left->successor != 0) {
    absl::StrAppend(&news, " ", left->successor_name, " leads it now.");
    // If they're playing here, they see it at once.
    if (const auto next = members_.find(left->successor);
        next != members_.end()) {
      next->second.rank = protocol::RANK_LEADER;
      guild_news_.insert(left->successor);
    }
  }
  TellGuild(left->guild, news);
  Notify(id, left->ended ? absl::StrCat("You were the last in ", left->name,
                                        ", so it's no more.")
                         : absl::StrCat("You left ", left->name, "."));
}

void Server::SetRank(uint32_t id, uint32_t player, protocol::Rank rank) {
  const absl::StatusOr<Changed> changed = guilds_.SetRank(id, player, rank);
  if (!changed.ok()) {
    Refuse(id, changed.status());
    return;
  }
  if (const auto member = members_.find(player); member != members_.end()) {
    member->second.rank = rank;
  }
  RefreshGuild(changed->guild);
  TellGuild(changed->guild,
            absl::StrCat(changed->player_name, rank == protocol::RANK_OFFICER
                                                   ? " is an officer now."
                                                   : " is a member now."));
}

void Server::Kick(uint32_t id, uint32_t player) {
  const absl::StatusOr<Changed> kicked = guilds_.Kick(id, player);
  if (!kicked.ok()) {
    Refuse(id, kicked.status());
    return;
  }
  // If they're playing here, they hear it, and lose the name over their head.
  if (members_.erase(player) > 0) {
    guild_news_.insert(player);
    Notify(player,
           absl::StrCat("You were removed from ", kicked->guild_name, "."));
  }
  RefreshGuild(kicked->guild);
  TellGuild(kicked->guild, absl::StrCat(WorldOf(id)->players().at(id).name,
                                        " removed ", kicked->player_name,
                                        " from ", kicked->guild_name, "."));
}

void Server::Disband(uint32_t id) {
  const absl::StatusOr<Membership> ended = guilds_.Disband(id);
  if (!ended.ok()) {
    Refuse(id, ended.status());
    return;
  }
  const std::string news = absl::StrCat(WorldOf(id)->players().at(id).name,
                                        " disbanded ", ended->name, ".");
  TellGuild(ended->guild, news);
  // Everyone in it who's playing here is in no guild now.
  std::erase_if(members_, [&](const auto& member) {
    if (member.second.guild != ended->guild) {
      return false;
    }
    guild_news_.insert(member.first);
    return true;
  });
}

void Server::GuildSay(uint32_t id, const std::string& text) {
  const auto member = members_.find(id);
  if (member == members_.end()) {
    Notify(id, "You're in no guild.");
    return;
  }
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
  protocol::World said;
  protocol::Chat* line = said.add_chat();
  line->set_name(WorldOf(id)->players().at(id).name);
  line->set_text(*clean);
  line->set_channel(protocol::CHANNEL_GUILD);
  std::cout << "[" << member->second.name << "] " << line->name() << ": "
            << line->text() << std::endl;
  // Serialized once, for all of them, like the world's chat.
  const auto bytes =
      std::make_shared<const std::string>(said.SerializeAsString());
  for (const auto& [other, membership] : members_) {
    if (membership.guild == member->second.guild) {
      metrics_.AddBytesSent(kHeaderSize + bytes->size());
      sessions_.at(other)->Send(bytes, {});
    }
  }
}

void Server::RefreshGuild(uint32_t guild) {
  for (const auto& [id, membership] : members_) {
    if (membership.guild == guild) {
      guild_news_.insert(id);
    }
  }
}

void Server::TellGuild(uint32_t guild, const std::string& text) {
  for (const auto& [id, membership] : members_) {
    if (membership.guild == guild) {
      Notify(id, text);
    }
  }
}

void Server::Refuse(uint32_t id, const absl::Status& why) {
  if (absl::IsUnavailable(why) || absl::IsInternal(why) ||
      absl::IsAborted(why)) {
    std::cerr << "a guild action of player " << id << " failed: " << why
              << "\n";
    Notify(id, "That didn't work. Try again.");
    return;
  }
  Notify(id, std::string(why.message()));
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

int Server::HandOver(const std::string& host, uint16_t port) {
  // The next instance loads each player from Redis, so they go in now, and
  // nothing changes them here afterwards. Those in a dungeon come out of it
  // there, as its copy stays here.
  handing_over_ = true;
  std::error_code ignored;
  acceptor_.close(ignored);
  SaveChanged();
  // The next instance turns away a player whose lease this one has, so their
  // leases go to it first, with newer fencing numbers. It's named as in
  // Consul, by its world and its port.
  const absl::StatusOr<std::set<uint32_t>> passed =
      players_.Pass(name_, leases_, InstanceId({.name = name_, .port = port}));
  if (!passed.ok()) {
    std::cerr << "couldn't hand the leases over: " << passed.status() << "\n";
    return 0;
  }
  int told = 0;
  for (const auto& [id, session] : sessions_) {
    if (!passed->contains(id)) {
      std::cerr << "couldn't hand player " << id
                << " over: their lease isn't this instance's\n";
      continue;
    }
    // It's the next instance's now, which leaving here leaves alone.
    leases_.erase(id);
    // Tokens are one-time, and the player's own is used, so each move gets a
    // token of its own.
    const absl::StatusOr<std::string> token = tokens_.Issue(id);
    if (!token.ok()) {
      std::cerr << "couldn't hand player " << id << " over: " << token.status()
                << "\n";
      continue;
    }
    protocol::World move;
    move.mutable_move()->set_host(host);
    move.mutable_move()->set_port(port);
    move.mutable_move()->set_token(*token);
    session->Send(Frame(move));
    ++told;
  }
  return told;
}

void Server::Tick() {
  ZoneScoped;
  if (handing_over_) {
    return;
  }
  const auto start = std::chrono::steady_clock::now();
  ApplyInputs();
  copies_.Step(interval_);
  for (const Notice& notice : copies_.TakeNotices()) {
    if (sessions_.contains(notice.player)) {
      Notify(notice.player, notice.text);
    }
  }
  const std::vector<Event> happened = copies_.TakeEvents();
  RecordEvents(happened);
  std::array<std::size_t, kLands.size()> in_land{};
  std::array<std::size_t, kLands.size()> copies_of{};
  for (const auto& [id, copy] : copies_.all()) {
    in_land[copy.land] += copy.world.players().size();
    ++copies_of[copy.land];
  }
  for (std::size_t land = 0; land < kLands.size(); ++land) {
    metrics_.SetPlayers(kLands[land].name, in_land[land]);
    metrics_.SetCopies(kLands[land].name, copies_of[land]);
  }
  SaveChanged();
  RenewLeases();
  Broadcast();
  ShowEvents(happened);
  SendChat();
  SendBags();
  SendQuests();
  SendGuilds();
  SendTrades();
  metrics_.ObserveTick(std::chrono::steady_clock::now() - start);
  TracyPlot("players", static_cast<int64_t>(players()));
  FrameMark;
  // From the last expiry, not from now, so that the ticks don't drift.
  timer_.expires_at(timer_.expiry() + interval_);
  timer_.async_wait([this](std::error_code) { Tick(); });
}

void Server::ApplyInputs() {
  inputs_.Apply([this](uint32_t id) { return WorldOf(id); });
}

void Server::SaveChanged() {
  ZoneScoped;
  const auto start = std::chrono::steady_clock::now();
  std::vector<uint32_t> everyone;
  for (const auto& [copy_id, copy] : copies_.all()) {
    for (const auto& [id, player] : copy.world.players()) {
      everyone.push_back(id);
    }
  }
  Save(everyone);
  metrics_.ObserveSave(std::chrono::steady_clock::now() - start);
}

void Server::RenewLeases() {
  const auto now = std::chrono::steady_clock::now();
  if (now - renewed_ < kRenewEvery) {
    return;
  }
  renewed_ = now;
  // All of them in one round trip, as the tick waits for it.
  const absl::Status renewed = players_.Renew(name_, leases_);
  if (!renewed.ok()) {
    std::cerr << "couldn't renew " << leases_.size() << " leases: " << renewed
              << "\n";
  }
}

void Server::Save(const std::vector<uint32_t>& ids) {
  std::map<uint32_t, Progress> changed;
  std::map<uint32_t, protocol::Inventory> bags;
  for (const uint32_t id : ids) {
    const Player& player = WorldOf(id)->players().at(id);
    const Progress progress = ProgressOf(player);
    if (progress != saved_[id]) {
      // The bag goes too only when its revision has moved.
      if (progress.items_revision != saved_[id].items_revision) {
        bags[id] = player.inventory.items();
      }
      changed[id] = progress;
    }
  }
  const absl::Status status = players_.Save(name_, changed, bags);
  if (status.ok()) {
    for (const auto& [id, progress] : changed) {
      saved_[id] = progress;
    }
  } else {
    std::cerr << "couldn't save " << changed.size() << " players: " << status
              << "\n";
  }
}

void Server::RecordEvents(const std::vector<Event>& happened) {
  ZoneScoped;
  if (happened.empty()) {
    return;
  }
  const absl::Time now = absl::Now();
  std::vector<std::string> rows;
  rows.reserve(happened.size());
  for (const Event& event : happened) {
    if (ForAnalytics(event.kind)) {
      rows.push_back(Row(event, copies_.content(), name_, now));
    }
  }
  if (rows.empty()) {
    return;
  }
  // The game goes on without its analytics.
  const absl::Status added = events_.Add(rows);
  if (!added.ok()) {
    std::cerr << "couldn't record " << rows.size() << " events: " << added
              << "\n";
  }
}

void Server::ShowEvents(const std::vector<Event>& happened) {
  ZoneScoped;
  // Each player's events go together, in a message a tick.
  std::map<uint32_t, protocol::World> shown;
  for (const Event& event : happened) {
    const std::optional<protocol::Event> for_players = ForPlayers(event);
    const auto copy = copies_.all().find(event.copy);
    if (!for_players || copy == copies_.all().end()) {
      continue;
    }
    const protocol::Event& seen = *for_players;
    const Cell cell = CellOf(event.x, event.z);
    for (const auto& [id, player] : copy->second.world.players()) {
      if (sessions_.contains(id) && Near(CellOf(player.x, player.z), cell)) {
        *shown[id].add_events() = seen;
      }
    }
  }
  for (const auto& [id, events] : shown) {
    std::string frame = Frame(events);
    metrics_.AddBytesSent(frame.size());
    sessions_.at(id)->Send(std::move(frame));
  }
}

void Server::Broadcast() {
  ZoneScoped;
  // Names, kinds, levels, weapons and the most health a unit can have change
  // now and then, and NPCs never do, so they go out once a second, and the
  // clients keep them in between.
  const bool full = broadcasts_ == 0;
  broadcasts_ = (broadcasts_ + 1) % kFullEvery;
  for (const auto& [id, copy] : copies_.all()) {
    BroadcastCopy(copy, full);
  }
}

void Server::BroadcastCopy(const Copy& copy, bool full) {
  const World& land = copy.world;
  // A dungeon's copy waits for its player with nobody to send it to.
  if (land.players().empty()) {
    return;
  }
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
      } else if (const auto member = members_.find(id);
                 member != members_.end()) {
        // Their guild goes with their name, once a second.
        entry.set_guild(member->second.name);
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
       Views(land.players(), land.monsters(), copy.npc_cells)) {
    for (const uint32_t id : view.here) {
      const auto session = sessions_.find(id);
      if (session == sessions_.end()) {
        continue;
      }
      // Each client's world is its own now, as each has a world of its own
      // whole, and what's changed since differs.
      std::string still;
      std::vector<std::string_view> pieces =
          Delta(id, copy, view, players, monsters, sent, still);
      if (full) {
        for (const uint32_t npc : view.npcs) {
          pieces.push_back(npcs.at(npc));
        }
      }
      const protocol::World you =
          YouIn(id, land.players().at(id), seed_ + kLands[copy.land].seed,
                copy.land, inputs_.acked(id));
      std::string own = you.SerializeAsString();
      const auto udp = udp_clients_.find(id);
      if (udp != udp_clients_.end()) {
        // A datagram arrives whole or not at all, so it needs no size in front.
        for (std::string& part : Split(pieces, sent)) {
          const auto bytes =
              std::make_shared<const std::string>(std::move(part));
          metrics_.AddBytesSent(bytes->size() + own.size());
          SendTo(udp->second, bytes, own);
        }
      } else {
        protocol::World stamp;
        stamp.set_sent(sent);
        std::string bytes = stamp.SerializeAsString();
        for (const std::string_view piece : pieces) {
          bytes.append(piece);
        }
        const auto whole =
            std::make_shared<const std::string>(std::move(bytes));
        metrics_.AddBytesSent(kHeaderSize + whole->size() + own.size());
        session->second->Send(whole, std::move(own));
      }
    }
  }
}

std::vector<std::string_view> Server::Delta(
    uint32_t id, const Copy& copy, const View& view,
    const std::map<uint32_t, std::string>& players,
    const std::map<uint32_t, std::string>& monsters, int64_t sent,
    std::string& still) {
  std::map<int64_t, Sent>& history = history_[id];
  // The world the client has whole, if we still have what we sent in it,
  // and it was in this copy of the land.
  const auto seen = seen_.find(id);
  auto baseline = history.end();
  if (seen != seen_.end()) {
    const auto found = history.find(seen->second);
    if (found != history.end() && found->second.copy == &copy) {
      baseline = found;
    }
  }
  Sent& now = history[sent];
  now.copy = &copy;
  std::vector<std::string_view> pieces;
  // Whether the piece has changed since the world the client has, which it
  // then needs. Either way, this world has it. Each world sent since has to
  // have had it the same too, as the client may have had those: one that
  // changed and changed back would leave it with the change.
  const auto changed = [&](uint64_t key, std::string_view piece) {
    const std::size_t hash = std::hash<std::string_view>{}(piece);
    now.pieces[key] = hash;
    bool same = baseline != history.end();
    // The end too, should the clock have stepped back past the client's.
    for (auto world = baseline;
         same && world != history.end() && world->first != sent; ++world) {
      const auto before = world->second.pieces.find(key);
      same = world->second.copy == &copy &&
             before != world->second.pieces.end() && before->second == hash;
    }
    if (same) {
      return false;
    }
    pieces.push_back(piece);
    return true;
  };
  protocol::World unchanged;
  for (const uint32_t player : view.players) {
    if (!changed(player, players.at(player))) {
      unchanged.add_still_players(player);
    }
  }
  // A monster's id goes above a player's, as the two count apart.
  for (const uint32_t monster : view.monsters) {
    if (!changed(uint64_t{1} << 32 | monster, monsters.at(monster))) {
      unchanged.add_still_monsters(monster);
    }
  }
  if (unchanged.still_players_size() + unchanged.still_monsters_size() > 0) {
    still = unchanged.SerializeAsString();
    pieces.push_back(still);
  }
  // The client never asks for one older than the world it has, and a second
  // of them is plenty.
  if (seen != seen_.end()) {
    history.erase(history.begin(), history.lower_bound(seen->second));
  }
  while (history.size() > kMostRemembered) {
    history.erase(history.begin());
  }
  return pieces;
}

void Server::SendChat() {
  if (chat_.chat().empty()) {
    return;
  }
  // Each line has to arrive, so it goes over TCP, even to a client that gets
  // the world over UDP. Everyone in the world hears it, in all its lands.
  const auto bytes =
      std::make_shared<const std::string>(chat_.SerializeAsString());
  for (const auto& [id, session] : sessions_) {
    metrics_.AddBytesSent(kHeaderSize + bytes->size());
    session->Send(bytes, {});
  }
  chat_.Clear();
}

void Server::SendBags() {
  for (const auto& [copy_id, copy] : copies_.all()) {
    for (const auto& [id, player] : copy.world.players()) {
      const uint32_t revision = player.inventory.revision();
      const auto session = sessions_.find(id);
      // A bag changes now and then, and has to arrive, so it goes on its own,
      // over TCP, only when it has.
      if (session == sessions_.end() || sent_bags_[id] == revision) {
        continue;
      }
      std::string frame = Frame(BagOf(player));
      metrics_.AddBytesSent(frame.size());
      session->second->Send(std::move(frame));
      sent_bags_[id] = revision;
    }
  }
}

void Server::SendQuests() {
  for (const auto& [copy_id, copy] : copies_.all()) {
    for (const auto& [id, player] : copy.world.players()) {
      const auto session = sessions_.find(id);
      const auto sent = sent_quests_.find(id);
      // Like a bag, but each player gets theirs as they join, none included,
      // as a quest's revision starts again from zero with each login.
      if (session == sessions_.end() ||
          (sent != sent_quests_.end() &&
           sent->second == player.quests_revision)) {
        continue;
      }
      std::string frame = Frame(QuestsOf(player));
      metrics_.AddBytesSent(frame.size());
      session->second->Send(std::move(frame));
      sent_quests_[id] = player.quests_revision;
    }
  }
}

void Server::SendGuilds() {
  // Each guild's roster is read once, for all its players who get news.
  std::map<uint32_t, std::vector<protocol::GuildMember>> rosters;
  for (const uint32_t id : guild_news_) {
    const auto session = sessions_.find(id);
    if (session == sessions_.end()) {
      continue;
    }
    // An empty guild, for none, still says so.
    protocol::World news;
    protocol::Guild* guild = news.mutable_guild();
    if (const auto member = members_.find(id); member != members_.end()) {
      guild->set_name(member->second.name);
      guild->set_rank(member->second.rank);
      auto roster = rosters.find(member->second.guild);
      if (roster == rosters.end()) {
        roster =
            rosters.emplace(member->second.guild, Roster(member->second.guild))
                .first;
      }
      guild->mutable_members()->Add(roster->second.begin(),
                                    roster->second.end());
    }
    std::string frame = Frame(news);
    metrics_.AddBytesSent(frame.size());
    session->second->Send(std::move(frame));
  }
  guild_news_.clear();
}

void Server::SendTrades() {
  // A trade has to arrive, as a bag does, so it goes on its own, over TCP, to
  // each side, and with nobody in it once it's over.
  for (const uint32_t id : copies_.TakeTradeNews()) {
    const auto session = sessions_.find(id);
    const World* land = WorldOf(id);
    if (session == sessions_.end() || land == nullptr) {
      continue;
    }
    protocol::World news;
    *news.mutable_trade() = land->TradeOf(id);
    std::string frame = Frame(news);
    metrics_.AddBytesSent(frame.size());
    session->second->Send(std::move(frame));
  }
}

std::vector<protocol::GuildMember> Server::Roster(uint32_t guild) {
  absl::StatusOr<std::vector<protocol::GuildMember>> roster =
      guilds_.Roster(guild);
  if (!roster.ok()) {
    std::cerr << "couldn't read guild " << guild
              << "'s roster: " << roster.status() << "\n";
    return {};
  }
  for (protocol::GuildMember& member : *roster) {
    if (World* land = WorldOf(member.id()); land != nullptr) {
      member.set_online(true);
      member.set_level(static_cast<uint32_t>(
          LevelFor(land->players().at(member.id()).experience).number));
    }
  }
  return *std::move(roster);
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
