#include "bot/bot.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_join.h"
#include "asio/buffer.hpp"
#include "asio/connect.hpp"
#include "asio/error.hpp"
#include "asio/ssl/stream_base.hpp"
#include "asio/write.hpp"
#include "bot/bag.h"
#include "protocol/nanommo.pb.h"
#include "terrain/lands.h"

namespace nanommo {

namespace {

// What a bot says now and then, when it gets where it was going.
constexpr std::array<const char*, 6> kLines = {
    "hi all",
    "anyone up for the Ashlands?",
    "careful, skeletons over here",
    "nice view from up here",
    "where is everyone going?",
    "brb",
};

// What a bot asks the storyteller for a story about: three words from the
// game, which the model knows.
constexpr std::array<const char*, 12> kWords = {
    "sword",  "gate",   "skeleton", "meadow", "ash",    "knight",
    "dragon", "castle", "forest",   "river",  "island", "gold",
};

constexpr double kAskForAStory = 0.125;

// While it wanders around somewhere, a bot uses a skill it knows with one
// direction in four, as players do in a fight.
constexpr double kUseASkill = 0.25;

// A potion's cooldown, so that a bot doesn't ask for one the server refuses.
constexpr std::chrono::seconds kDrinkEvery{10};

// How close a bot gets for an errand, in metres. For a monster, near enough to
// swing at it: the server runs it in the rest of the way, as it does a player
// who clicks one, so a big monster that moves, like a boss, still gets hit.
// For an NPC, within the four metres it hears you from.
constexpr float kSwing = 6.0f;
constexpr float kTalk = 3.0f;
// The longest a bot at war waits at its camp for the rest of its side.
constexpr auto kMuster = std::chrono::seconds(90);

// How soon a bot thinks again once the monster it hunts is dead: soon, but
// not before what it sent last has gone out.
constexpr std::chrono::milliseconds kRethink{100};

std::string RandomWords(std::mt19937& random) {
  std::vector<const char*> words;
  std::ranges::sample(kWords, std::back_inserter(words), 3, random);
  return absl::StrJoin(words, " ");
}

}  // namespace

Bot::Bot(asio::io_context& io, asio::ssl::context& tls, uint32_t seed, bool udp,
         const Content& content, std::optional<Side> side)
    : stream_(io, tls),
      udp_socket_(io),
      timer_(io),
      hello_timer_(io),
      tick_timer_(io),
      random_(seed),
      udp_(udp),
      content_(content),
      side_(std::move(side)) {}

void Bot::Start(const asio::ip::tcp::resolver::results_type& server,
                const std::string& token) {
  // The resolver keeps the name we looked up, which the certificate must have.
  const std::string host = server.begin()->host_name();
  ExpectName(stream_, host);
  asio::async_connect(
      stream_.lowest_layer(), server,
      [this, token, host](std::error_code error, const auto&) {
        if (error) {
          std::cerr << "nanommo bot: couldn't connect: " << error.message()
                    << '\n';
          return;
        }
        std::error_code ignored;
        stream_.lowest_layer().set_option(asio::ip::tcp::no_delay(true),
                                          ignored);
        stream_.async_handshake(asio::ssl::stream_base::client,
                                [this, token, host](std::error_code error) {
                                  Greet(token, host, error);
                                });
      });
}

void Bot::Greet(const std::string& token, const std::string& host,
                std::error_code error) {
  if (error) {
    std::cerr << "nanommo bot: couldn't shake hands: "
              << HandshakeError(stream_, host, error) << '\n';
    return;
  }
  // Blocking is fine: a hello this small fits in the socket's buffer.
  protocol::Hello hello;
  hello.set_token(token);
  asio::write(stream_, asio::buffer(Frame(hello)), error);
  if (error) {
    return;
  }
  connected_ = true;
  Read();
  Wander();
  Tick();
  if (udp_) {
    const asio::ip::tcp::endpoint server =
        stream_.lowest_layer().remote_endpoint(error);
    if (!error) {
      udp_socket_.connect({server.address(), server.port()}, error);
    }
    if (!error) {
      token_ = token;
      HelloOverUdp();
      ReceiveOverUdp();
    }
  }
}

// Reading every world keeps the server's writes to us from backing up.
void Bot::Read() {
  stream_.async_read_some(asio::buffer(buffer_),
                          [this](std::error_code error, std::size_t size) {
                            if (error) {
                              Stop();
                              return;
                            }
                            reader_.Append({buffer_.data(), size});
                            while (const auto message = reader_.Next()) {
                              Receive(*message);
                            }
                            if (!reader_.status().ok()) {
                              Stop();
                              return;
                            }
                            Read();
                          });
}

void Bot::HelloOverUdp() {
  if (!connected_ || udp_worlds_) {
    return;
  }
  protocol::Hello hello;
  hello.set_token(token_);
  std::error_code ignored;
  udp_socket_.send(asio::buffer(hello.SerializeAsString()), 0, ignored);
  hello_timer_.expires_after(std::chrono::milliseconds(250));
  hello_timer_.async_wait([this](std::error_code error) {
    if (!error) {
      HelloOverUdp();
    }
  });
}

void Bot::ReceiveOverUdp() {
  udp_socket_.async_receive(
      asio::buffer(datagram_), [this](std::error_code error, std::size_t size) {
        if (!connected_ || error == asio::error::operation_aborted) {
          return;
        }
        if (!error) {
          udp_worlds_ = true;
          Receive({datagram_.data(), size});
        }
        ReceiveOverUdp();
      });
}

void Bot::Receive(std::string_view bytes) {
  protocol::World world;
  // Chat and events have no land in them, which would read as the Meadows,
  // and neither does a move. The bot goes by the world, so it skips events.
  if (!world.ParseFromArray(bytes.data(), static_cast<int>(bytes.size())) ||
      !world.chat().empty() || !world.events().empty()) {
    return;
  }
  if (world.has_move()) {
    move_ = world.move();
    return;
  }
  // Our bag comes on its own too, with nothing of the world in it.
  if (world.has_inventory()) {
    bag_ = world.inventory();
    return;
  }
  if (world.has_quests()) {
    quests_ = world.quests();
    return;
  }
  // A bot doesn't say yes to a duel, nor to a guild that asks it in, unless
  // it's at war and that's its side's guild. It keeps its guild's news, for
  // the war.
  if (world.has_guild()) {
    guild_ = world.guild();
    return;
  }
  if (world.has_invite()) {
    asked_in_ = side_ && world.invite().guild() == side_->guild;
    return;
  }
  if (world.has_duel()) {
    return;
  }
  parts_.Add(world);
  // Once the monster it hunts is dead, the bot thinks again, and doesn't swing
  // at the body for as long as it holds its input.
  if (hunting_ != 0 && Dead(world, hunting_)) {
    hunting_ = 0;
    WanderIn(kRethink);
  }
  // And so does one at war once the enemy it's after is dead.
  if (enemy_ != 0 &&
      std::ranges::any_of(world.players(),
                          [this](const protocol::Player& player) {
                            return player.id() == enemy_ &&
                                   player.motion() == protocol::MOTION_DEAD;
                          })) {
    enemy_ = 0;
    WanderIn(kRethink);
  }
  const uint32_t parts = std::max(world.parts(), 1u);
  newest_.resize(parts);
  if (world.part() < parts) {
    newest_[world.part()] = std::max(newest_[world.part()], world.sent());
  }
  for (const protocol::Player& player : world.players()) {
    if (player.id() == world.you()) {
      x_ = player.x();
      z_ = player.z();
      dead_ = player.motion() == protocol::MOTION_DEAD;
      can_respawn_ = dead_ && world.respawn() == 0;
      health_ = static_cast<int>(player.health());
      // The most comes once a second, with the names.
      if (player.max_health() != 0) {
        max_health_ = static_cast<int>(player.max_health());
      }
    }
  }
  skills_ = {world.skills().begin(), world.skills().end()};
  // In a monster's skill on its way, the bot runs out of it, as players learn
  // to, and doesn't think of anything else until it's clear. Then it thinks
  // again from where it is: the trip it was on counted on getting closer, and
  // a boss's skills come one after another.
  if (const std::optional<protocol::Input> out =
          dead_ ? std::nullopt
                : StepOut(content_.monsters, parts_.world(), x_, z_)) {
    if (!stepping_out_) {
      hunting_ = 0;
      trip_.reset();
      errand_.reset();
      Send(*out);
    }
    stepping_out_ = true;
    WanderIn(kRethink);
  } else {
    stepping_out_ = false;
  }
  // Through a gate: the trip was for the land we left.
  if (world.land() != land_ && world.land() < kLands.size()) {
    land_ = world.land();
    trip_.reset();
    storyteller_.reset();
    asking_ = false;
    errand_.reset();
  }
  for (const protocol::Npc& npc : world.npcs()) {
    const auto kind = content_.npcs.kinds.find(npc.kind());
    if (kind != content_.npcs.kinds.end() && kind->second.talks()) {
      storyteller_ = npc;
    }
  }
}

void Bot::Wander() {
  hunting_ = 0;
  enemy_ = 0;
  // At war, a bot does one thing at a time about its guild, with its keys as
  // they are: says yes to its side's, once it's asked, or as its leader,
  // founds it, takes it to war and asks the rest of its side in.
  if (side_) {
    std::optional<protocol::GuildAction> action;
    if (asked_in_) {
      asked_in_ = false;
      action.emplace().set_accept(true);
    } else {
      action = Enlist(*side_, guild_, recruit_);
    }
    if (action) {
      protocol::Input keys = keys_;
      *keys.mutable_guild() = *action;
      Send(keys);
    }
  }
  // A dead bot comes back as soon as it can, as most players do.
  if (dead_) {
    if (can_respawn_) {
      protocol::Input respawn;
      respawn.set_respawn(true);
      Send(respawn);
    }
    WanderIn(std::chrono::seconds(1));
    return;
  }
  // At war, a bot waits at its side's camp until all its side is in the
  // guild, so that the two sides charge each other in lines, rather than
  // fighting where each of them comes into the world. A minute and a half
  // after it joined, it goes anyway, for a side whose bots don't all log in.
  if (side_ && !charged_) {
    const auto now = std::chrono::steady_clock::now();
    const bool in = guild_.name() == side_->guild;
    if (in && !joined_) {
      joined_ = now;
    }
    charged_ = (in && guild_.members_size() >= side_->size) ||
               (joined_ && now - *joined_ >= kMuster);
    if (!charged_) {
      Trip camp{.to_x = side_->camp_x, .to_z = side_->camp_z, .there = 1.0f};
      Send(Travel(camp, x_, z_).value_or(protocol::Input()));
      WanderIn(std::chrono::seconds(1));
      return;
    }
  }
  // In its guild, it goes for the nearest enemy it can see, before anything
  // else, and the server runs it up to them.
  if (side_ && guild_.name() == side_->guild) {
    enemy_ = NearestEnemy(*side_, parts_.world(), x_, z_);
  }
  if (enemy_ != 0) {
    trip_.reset();
    errand_.reset();
    protocol::Input attack;
    attack.set_attack(true);
    attack.set_target_player(enemy_);
    Send(attack);
    WanderIn(std::chrono::seconds(1));
    return;
  }
  // Quests come first: a bot hunts what they ask for, and takes and turns them
  // in, as players do, whenever its world has some of that around it.
  if (!trip_ && stay_ == 0) {
    errand_ = NextErrand(content_, quests_, parts_.world(), x_, z_);
    if (errand_) {
      trip_ =
          Trip{.to_x = errand_->x,
               .to_z = errand_->z,
               .there = errand_->kind == Errand::Kind::kHunt ? kSwing : kTalk};
    }
  }
  // Otherwise, like players, a bot goes somewhere on the island, and stays
  // around there for a while. One trip in eight goes to the storyteller, once
  // the bot has seen it, for a story.
  if (!trip_ && stay_ == 0) {
    asking_ =
        storyteller_ && std::bernoulli_distribution(kAskForAStory)(random_);
    trip_ = asking_ ? Trip{.to_x = storyteller_->x(), .to_z = storyteller_->z()}
                    : RandomTrip(random_,
                                 RandomGate(random_, static_cast<int>(land_)));
  }
  std::optional<protocol::Input> input;
  std::string say;
  std::optional<protocol::Talk> talk;
  if (trip_) {
    input = Travel(*trip_, x_, z_);
    if (!input && errand_) {
      // There, unless it stopped getting closer, at the sea or a lake, when it
      // wanders around for a while instead. The next errand goes after
      // whatever's nearest then, as monsters move.
      if (std::hypot(trip_->to_x - x_, trip_->to_z - z_) < trip_->there) {
        input.emplace();
        if (errand_->kind == Errand::Kind::kHunt) {
          input->set_attack(true);
          // Picked first, as a player clicks it, so the swings go for it.
          input->set_target(errand_->monster);
          hunting_ = errand_->monster;
        } else if (errand_->kind == Errand::Kind::kTake) {
          *input->mutable_take() = errand_->quest;
        } else {
          *input->mutable_turn_in() = errand_->quest;
        }
      } else {
        stay_ = std::uniform_int_distribution<int>(5, 15)(random_);
      }
      trip_.reset();
      errand_.reset();
    } else if (!input) {
      trip_.reset();
      stay_ = std::uniform_int_distribution<int>(5, 15)(random_);
      if (asking_ && storyteller_) {
        asking_ = false;
        talk.emplace();
        talk->set_npc(storyteller_->id());
        talk->set_words(RandomWords(random_));
      } else if (std::bernoulli_distribution(0.25)(random_)) {
        say = kLines[std::uniform_int_distribution<std::size_t>(
            0, kLines.size() - 1)(random_)];
      }
    }
  }
  if (!input) {
    --stay_;
    input = RandomDirection(random_);
    if (!skills_.empty() && std::bernoulli_distribution(kUseASkill)(random_)) {
      input->set_skill(skills_[std::uniform_int_distribution<std::size_t>(
          0, skills_.size() - 1)(random_)]);
    }
  }
  // As a player would, a bot drinks a potion when it's hurt, and wears what
  // it finds, which keeps the servers saving and sending bags.
  const auto now = std::chrono::steady_clock::now();
  if (const std::optional<protocol::ItemAction> item =
          Tend(content_.items, bag_, health_, max_health_,
               now - drank_ >= kDrinkEvery)) {
    *input->mutable_item() = *item;
    if (item->has_use()) {
      drank_ = now;
    }
  }
  input->set_say(say);
  if (talk) {
    *input->mutable_talk() = *talk;
  }
  Send(*input);
  WanderIn(trip_ ? std::chrono::seconds(1) : RandomHold(random_));
}

void Bot::Send(const protocol::Input& input) {
  keys_.set_x(input.x());
  keys_.set_z(input.z());
  keys_.set_run(input.run());
  keys_.set_attack(input.attack());
  keys_.set_target(input.target());
  keys_.set_target_player(input.target_player());
  // With the newest world it has whole, as the client does.
  protocol::Input with = input;
  with.set_seen(parts_.whole());
  Write(Frame(with));
}

void Bot::Tick() {
  if (!connected_) {
    return;
  }
  // The keys alone, so that nothing else it did happens again.
  protocol::Input keys = keys_;
  keys.set_seen(parts_.whole());
  Write(Frame(keys));
  tick_timer_.expires_after(std::chrono::milliseconds(50));
  tick_timer_.async_wait([this](std::error_code error) {
    if (!error) {
      Tick();
    }
  });
}

void Bot::Write(std::string frame) {
  outbox_.push_back(std::move(frame));
  // One write at a time on a stream: the next starts as this one ends.
  if (outbox_.size() > 1) {
    return;
  }
  const auto next = [this](const auto& self) -> void {
    asio::async_write(stream_, asio::buffer(outbox_.front()),
                      [this, self](std::error_code error, std::size_t) {
                        outbox_.pop_front();
                        if (error) {
                          connected_ = false;
                          outbox_.clear();
                          return;
                        }
                        if (!outbox_.empty()) {
                          self(self);
                        }
                      });
  };
  next(next);
}

void Bot::WanderIn(std::chrono::milliseconds wait) {
  timer_.expires_after(wait);
  timer_.async_wait([this](std::error_code error) {
    if (!error && connected_) {
      Wander();
    }
  });
}

void Bot::Leave() {
  Stop();
  std::error_code ignored;
  stream_.lowest_layer().close(ignored);
}

void Bot::Stop() {
  connected_ = false;
  timer_.cancel();
  hello_timer_.cancel();
  tick_timer_.cancel();
  std::error_code ignored;
  udp_socket_.close(ignored);
}

}  // namespace nanommo
