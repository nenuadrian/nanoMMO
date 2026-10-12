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
#include "asio/ip/udp.hpp"
#include "asio/ssl/context.hpp"
#include "asio/steady_timer.hpp"
#include "protocol/framing.h"
#include "protocol/nanommo.pb.h"
#include "protocol/parts.h"
#include "protocol/tls.h"

namespace nanommo {

class Connection {
 public:
  /// Connects to the server over TLS, or returns why it couldn't.
  static absl::StatusOr<std::unique_ptr<Connection>> Create(
      const std::string& host, uint16_t port, asio::ssl::context& tls,
      const std::string& token, bool udp);

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  void Poll();

  void Send(const protocol::Input& input);

  bool connected() const { return connected_; }

  const protocol::World& world() const { return world_; }

 private:
  Connection(asio::ssl::context& tls, const std::string& token);

  absl::Status Start(const std::string& host, uint16_t port,
                     const std::string& token, bool udp);
  void Read();
  void HelloOverUdp();
  void ReceiveOverUdp();
  void Receive(const protocol::World& world);

  asio::io_context io_;
  TlsStream stream_;
  asio::ip::udp::socket udp_socket_;
  asio::steady_timer hello_timer_;
  std::string token_;
  std::array<char, 4096> buffer_;
  std::array<char, 2048> datagram_;
  FrameReader reader_;
  Parts parts_;
  protocol::World world_;
  bool udp_worlds_ = false;
  bool connected_ = true;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_CONNECTION_H_
