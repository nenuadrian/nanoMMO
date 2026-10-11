#ifndef NANOMMO_SERVER_SERVER_H_
#define NANOMMO_SERVER_SERVER_H_

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/steady_timer.hpp"
#include "server/session.h"
#include "server/world.h"

namespace nanommo {

class Server {
 public:
  /// Starts the server, or returns why it couldn't, such as the port
  /// being taken.
  static absl::StatusOr<std::unique_ptr<Server>> Create(
      asio::io_context& io, uint16_t port, std::chrono::nanoseconds interval);

 private:
  Server(asio::io_context& io, std::chrono::nanoseconds interval);

  absl::Status Start(uint16_t port);
  void Accept();
  void Tick();
  void Broadcast();

  asio::ip::tcp::acceptor acceptor_;
  asio::steady_timer timer_;
  std::chrono::nanoseconds interval_;
  World world_;
  std::map<uint32_t, std::shared_ptr<Session>> sessions_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_SERVER_H_
