#ifndef NANOMMO_SERVER_SERVER_H_
#define NANOMMO_SERVER_SERVER_H_

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/ip/udp.hpp"
#include "asio/ssl/context.hpp"
#include "asio/steady_timer.hpp"
#include "content/content.h"
#include "protocol/nanommo.pb.h"
#include "server/copies.h"
#include "server/interest.h"
#include "server/metrics.h"
#include "server/players.h"
#include "server/session.h"
#include "server/storyteller.h"
#include "server/token_bucket.h"
#include "server/tokens.h"
#include "server/world.h"

namespace nanommo {

class Server {
 public:
  /// Starts the server, or returns why it couldn't, such as the port
  /// being taken. Clients connect over TLS, with the context. The name keeps
  /// the world's players apart from other worlds', and the seed makes the
  /// terrain.
  static absl::StatusOr<std::unique_ptr<Server>> Create(
      asio::io_context& io, uint16_t port, asio::ssl::context& tls,
      std::chrono::nanoseconds interval, std::string name, uint32_t seed,
      const Content& content, Metrics& metrics, Tokens& tokens,
      Players& players, Storyteller& storyteller);

  /// In all its lands.
  int players() const;

  /// Hands the players over to the world's next instance, at the host and port
  /// players reach it at: saves them, stops ticking and taking players, and
  /// tells each client where to go. Returns how many it told.
  int HandOver(const std::string& host, uint16_t port);

  /// The port it listens on, over TCP and UDP. Given port zero, Create has the
  /// system pick a free one.
  uint16_t port() const;

 private:
  Server(asio::io_context& io, asio::ssl::context& tls,
         std::chrono::nanoseconds interval, std::string name, uint32_t seed,
         const Content& content, Metrics& metrics, Tokens& tokens,
         Players& players, Storyteller& storyteller);

  absl::Status Start(uint16_t port);
  /// The World of the copy the player's in, or nullptr.
  World* WorldOf(uint32_t id);
  void Accept();
  uint32_t HandleHello(Session& session, const std::string& bytes);
  void Leave(uint32_t id);
  void Say(uint32_t id, const std::string& text);
  void Talk(uint32_t id, const protocol::Talk& talk);
  void Learn(uint32_t id, const protocol::Learn& learn);
  /// Sends the player a line from the server, which has no name.
  void Notify(uint32_t id, const std::string& text);
  void Receive();
  void Tick();
  void SaveChanged();
  void Broadcast();
  void BroadcastCopy(const Copy& copy, bool full);
  void SendChat();
  void SendTo(const asio::ip::udp::endpoint& client,
              std::shared_ptr<const std::string> part, std::string own);

  asio::ip::tcp::acceptor acceptor_;
  asio::ssl::context& tls_;
  asio::ip::udp::socket udp_;
  std::array<char, 1024> datagram_;
  asio::ip::udp::endpoint sender_;
  asio::steady_timer timer_;
  std::chrono::nanoseconds interval_;
  std::string name_;
  uint32_t seed_;
  Copies copies_;
  Metrics& metrics_;
  Tokens& tokens_;
  Players& players_;
  Storyteller& storyteller_;
  std::map<uint32_t, std::shared_ptr<Session>> sessions_;
  std::map<std::string, uint32_t> udp_tokens_;
  std::map<uint32_t, asio::ip::udp::endpoint> udp_clients_;
  std::map<uint32_t, Progress> saved_;
  int broadcasts_ = 0;
  bool handing_over_ = false;
  /// What the players said since the last tick.
  protocol::World chat_;
  std::map<uint32_t, TokenBucket> chat_limits_;
  std::map<uint32_t, TokenBucket> story_limits_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_SERVER_H_
