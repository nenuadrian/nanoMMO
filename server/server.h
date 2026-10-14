#ifndef NANOMMO_SERVER_SERVER_H_
#define NANOMMO_SERVER_SERVER_H_

#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/ip/udp.hpp"
#include "asio/ssl/context.hpp"
#include "asio/steady_timer.hpp"
#include "server/metrics.h"
#include "server/players.h"
#include "server/session.h"
#include "server/tokens.h"
#include "server/world.h"

namespace nanommo {

class Server {
 public:
  /// Starts the server, or returns why it couldn't, such as the port
  /// being taken. Clients connect over TLS, with the context. The seed makes
  /// the terrain.
  static absl::StatusOr<std::unique_ptr<Server>> Create(
      asio::io_context& io, uint16_t port, asio::ssl::context& tls,
      std::chrono::nanoseconds interval, uint32_t seed, Metrics& metrics,
      Tokens& tokens, Players& players);

  int players() const { return static_cast<int>(world_.players().size()); }

 private:
  Server(asio::io_context& io, asio::ssl::context& tls,
         std::chrono::nanoseconds interval, uint32_t seed, Metrics& metrics,
         Tokens& tokens, Players& players);

  absl::Status Start(uint16_t port);
  void Accept();
  uint32_t HandleHello(Session& session, const std::string& bytes);
  void Leave(uint32_t id);
  void Receive();
  void Tick();
  void SaveChanged();
  void Broadcast();
  void SendTo(const asio::ip::udp::endpoint& client,
              std::shared_ptr<const std::string> part, std::string own);

  asio::ip::tcp::acceptor acceptor_;
  asio::ssl::context& tls_;
  asio::ip::udp::socket udp_;
  std::array<char, 1024> datagram_;
  asio::ip::udp::endpoint sender_;
  asio::steady_timer timer_;
  std::chrono::nanoseconds interval_;
  uint32_t seed_;
  World world_;
  Metrics& metrics_;
  Tokens& tokens_;
  Players& players_;
  std::map<uint32_t, std::shared_ptr<Session>> sessions_;
  std::map<std::string, uint32_t> udp_tokens_;
  std::map<uint32_t, asio::ip::udp::endpoint> udp_clients_;
  std::map<uint32_t, Progress> saved_;
  int broadcasts_ = 0;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_SERVER_H_
