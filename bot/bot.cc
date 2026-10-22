#include "bot/bot.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "absl/strings/str_join.h"
#include "asio/buffer.hpp"
#include "asio/connect.hpp"
#include "asio/error.hpp"
#include "asio/ssl/host_name_verification.hpp"
#include "asio/ssl/stream_base.hpp"
#include "asio/write.hpp"
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

std::string RandomWords(std::mt19937& random) {
  std::vector<const char*> words;
  std::ranges::sample(kWords, std::back_inserter(words), 3, random);
  return absl::StrJoin(words, " ");
}

}  // namespace

Bot::Bot(asio::io_context& io, asio::ssl::context& tls, uint32_t seed, bool udp,
         const NpcTable& npcs)
    : stream_(io, tls),
      udp_socket_(io),
      timer_(io),
      hello_timer_(io),
      random_(seed),
      udp_(udp),
      npcs_(npcs) {}

void Bot::Start(const asio::ip::tcp::resolver::results_type& server,
                const std::string& token) {
  // The resolver keeps the name we looked up, which the certificate must have.
  stream_.set_verify_callback(
      asio::ssl::host_name_verification(server.begin()->host_name()));
  asio::async_connect(
      stream_.lowest_layer(), server,
      [this, token](std::error_code error, const auto&) {
        if (error) {
          std::cerr << "nanommo bot: couldn't connect: " << error.message()
                    << '\n';
          return;
        }
        std::error_code ignored;
        stream_.lowest_layer().set_option(asio::ip::tcp::no_delay(true),
                                          ignored);
        stream_.async_handshake(
            asio::ssl::stream_base::client,
            [this, token](std::error_code error) { Greet(token, error); });
      });
}

void Bot::Greet(const std::string& token, std::error_code error) {
  if (error) {
    std::cerr << "nanommo bot: couldn't shake hands: " << error.message()
              << '\n';
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
  // Chat has no land in it, which would read as the Meadows.
  if (!world.ParseFromArray(bytes.data(), static_cast<int>(bytes.size())) ||
      !world.chat().empty()) {
    return;
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
    }
  }
  skills_ = {world.skills().begin(), world.skills().end()};
  // Through a gate: the trip was for the land we left.
  if (world.land() != land_ && world.land() < kLands.size()) {
    land_ = world.land();
    trip_.reset();
    storyteller_.reset();
    asking_ = false;
  }
  for (const protocol::Npc& npc : world.npcs()) {
    const auto kind = npcs_.kinds.find(npc.kind());
    if (kind != npcs_.kinds.end() && kind->second.talks()) {
      storyteller_ = npc;
    }
  }
}

void Bot::Wander() {
  // Like players, a bot goes somewhere on the island, and stays around there
  // for a while. One trip in eight goes to the storyteller, once the bot has
  // seen it, for a story.
  if (!trip_ && stay_ == 0) {
    asking_ =
        storyteller_ && std::bernoulli_distribution(kAskForAStory)(random_);
    trip_ = asking_ ? Trip{.to_x = storyteller_->x(), .to_z = storyteller_->z()}
                    : RandomTrip(random_, kLands[land_].gate);
  }
  std::optional<protocol::Input> input;
  std::string say;
  std::optional<protocol::Talk> talk;
  if (trip_) {
    input = Travel(*trip_, x_, z_);
    if (!input) {
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
  input->set_say(say);
  if (talk) {
    *input->mutable_talk() = *talk;
  }
  outgoing_ = Frame(*input);
  asio::async_write(stream_, asio::buffer(outgoing_),
                    [this](std::error_code error, std::size_t) {
                      if (error) {
                        connected_ = false;
                      }
                    });
  timer_.expires_after(trip_ ? std::chrono::seconds(1) : RandomHold(random_));
  timer_.async_wait([this](std::error_code error) {
    if (!error && connected_) {
      Wander();
    }
  });
}

void Bot::Stop() {
  connected_ = false;
  timer_.cancel();
  hello_timer_.cancel();
  std::error_code ignored;
  udp_socket_.close(ignored);
}

}  // namespace nanommo
