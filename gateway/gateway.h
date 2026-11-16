#ifndef NANOMMO_GATEWAY_GATEWAY_H_
#define NANOMMO_GATEWAY_GATEWAY_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "absl/status/statusor.h"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/ssl/context.hpp"

namespace nanommo {

/// A server the gateway connects a page to: where, and the name its
/// certificate has to have, as a client on a computer would check it.
struct Upstream {
  std::string host;
  uint16_t port = 0;
  std::string name;
};

/// Where a page's path leads, or why nowhere.
using Route = std::function<absl::StatusOr<Upstream>(std::string_view path)>;

/// Finds a world's own address by the host and port players reach it at.
using WorldAddress = std::function<absl::StatusOr<std::string>(
    std::string_view public_host, uint16_t port)>;

/// The gateway's two routes: /login to the login server, and
/// /world/<host>/<port> to a world, only where the lookup finds one, so the
/// gateway connects pages to our servers and nothing else.
Route Routes(Upstream login, WorldAddress world_address);

/// Lets a browser's page reach the servers, which a page can't open a socket
/// to: it takes WebSocket connections, and relays each one's messages to a
/// server over TLS, and the server's bytes back, as they are.
class Gateway {
 public:
  /// A page's message larger than this is refused, as the servers would.
  static constexpr std::size_t kMaxMessage = 64 * 1024;

  /// Listens on the port (0 for any), or returns why it can't.
  static absl::StatusOr<std::unique_ptr<Gateway>> Create(
      asio::io_context& io, uint16_t port, asio::ssl::context& tls,
      Route route);

  Gateway(const Gateway&) = delete;
  Gateway& operator=(const Gateway&) = delete;

  uint16_t port() const { return acceptor_.local_endpoint().port(); }

 private:
  Gateway(asio::io_context& io, asio::ssl::context& tls, Route route);

  void Accept();

  asio::ip::tcp::acceptor acceptor_;
  asio::ssl::context& tls_;
  Route route_;
};

}  // namespace nanommo

#endif  // NANOMMO_GATEWAY_GATEWAY_H_
