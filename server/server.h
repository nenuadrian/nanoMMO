#ifndef NANOMMO_SERVER_SERVER_H_
#define NANOMMO_SERVER_SERVER_H_

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
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
  /// being taken.
  static absl::StatusOr<std::unique_ptr<Server>> Create(
      asio::io_context& io, uint16_t port, std::chrono::nanoseconds interval,
      Metrics& metrics, Tokens& tokens, Players& players);

  void SaveAll();

 private:
  Server(asio::io_context& io, std::chrono::nanoseconds interval,
         Metrics& metrics, Tokens& tokens, Players& players);

  absl::Status Start(uint16_t port);
  void Accept();
  uint32_t HandleHello(Session& session, const std::string& bytes);
  void Leave(uint32_t id);
  void Tick();
  void Broadcast();

  asio::ip::tcp::acceptor acceptor_;
  asio::steady_timer timer_;
  std::chrono::nanoseconds interval_;
  World world_;
  Metrics& metrics_;
  Tokens& tokens_;
  Players& players_;
  std::map<uint32_t, std::shared_ptr<Session>> sessions_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_SERVER_H_
