#ifndef NANOMMO_BOT_BOT_H_
#define NANOMMO_BOT_BOT_H_

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/ip/udp.hpp"
#include "asio/ssl/context.hpp"
#include "asio/steady_timer.hpp"
#include "bot/wander.h"
#include "content/content.h"
#include "protocol/framing.h"
#include "protocol/nanommo.pb.h"
#include "protocol/tls.h"

namespace nanommo {

class Bot {
 public:
  /// Finds the NPCs that talk through their kinds in the content, and what
  /// its items do through the items.
  Bot(asio::io_context& io, asio::ssl::context& tls, uint32_t seed, bool udp,
      const Content& content);

  Bot(const Bot&) = delete;
  Bot& operator=(const Bot&) = delete;

  void Start(const asio::ip::tcp::resolver::results_type& server,
             const std::string& token);

  bool connected() const { return connected_; }

  /// Where the bot's world told it to go, once it hands its players over.
  const std::optional<protocol::Move>& move() const { return move_; }

  /// Closes the connection, as a player who goes elsewhere does.
  void Leave();

  const std::vector<int64_t>& newest() const { return newest_; }

 private:
  void Greet(const std::string& token, std::error_code error);
  void Read();
  void HelloOverUdp();
  void ReceiveOverUdp();
  void Receive(std::string_view bytes);
  void Wander();
  void Stop();

  TlsStream stream_;
  asio::ip::udp::socket udp_socket_;
  asio::steady_timer timer_;
  asio::steady_timer hello_timer_;
  std::mt19937 random_;
  bool udp_;
  const Content& content_;
  std::string token_;
  std::array<char, 65536> buffer_;
  std::array<char, 2048> datagram_;
  FrameReader reader_;
  std::vector<int64_t> newest_;
  bool udp_worlds_ = false;
  std::string outgoing_;
  bool connected_ = false;
  float x_ = 0.0f;
  float z_ = 0.0f;
  uint32_t land_ = 0;
  std::optional<Trip> trip_;
  int stay_ = 0;
  /// The storyteller in the bot's land, once the bot has seen it.
  std::optional<protocol::Npc> storyteller_;
  /// Whether the trip goes to the storyteller, for a story.
  bool asking_ = false;
  /// The skills the bot knows, from its world.
  std::vector<uint32_t> skills_;
  int health_ = 0;
  int max_health_ = 0;
  protocol::Inventory bag_;
  /// When it last drank something, which it does once its cooldown's over.
  std::chrono::steady_clock::time_point drank_;
  std::optional<protocol::Move> move_;
};

}  // namespace nanommo

#endif  // NANOMMO_BOT_BOT_H_
