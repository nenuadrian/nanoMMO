#ifndef NANOMMO_CLIENT_CONNECTION_H_
#define NANOMMO_CLIENT_CONNECTION_H_

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "protocol/framing.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

class Connection {
 public:
  /// Connects to the server, or returns why it couldn't.
  static absl::StatusOr<std::unique_ptr<Connection>> Create(
      const std::string& host, uint16_t port);

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  void Poll();

  void Send(const protocol::Input& input);

  bool connected() const { return connected_; }

  const protocol::World& world() const { return world_; }

 private:
  Connection();

  absl::Status Start(const std::string& host, uint16_t port);
  void Read();

  asio::io_context io_;
  asio::ip::tcp::socket socket_;
  std::array<char, 4096> buffer_;
  FrameReader reader_;
  protocol::World world_;
  bool connected_ = true;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_CONNECTION_H_
