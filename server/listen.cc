#include "server/listen.h"

#include <cstdint>
#include <system_error>

#include "absl/strings/str_cat.h"
#include "asio/socket_base.hpp"

namespace nanommo {

absl::Status Listen(asio::ip::tcp::acceptor& acceptor, uint16_t port) {
  // The steps the acceptor's constructor takes, with the overloads that hand
  // back an error code.
  const asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
  std::error_code error;
  acceptor.open(endpoint.protocol(), error);
  if (!error) {
    acceptor.set_option(asio::socket_base::reuse_address(true), error);
  }
  if (!error) {
    acceptor.bind(endpoint, error);
  }
  if (!error) {
    acceptor.listen(asio::socket_base::max_listen_connections, error);
  }
  if (error) {
    return absl::UnavailableError(
        absl::StrCat("couldn't listen on port ", port, ": ", error.message()));
  }
  return absl::OkStatus();
}

absl::Status Bind(asio::ip::udp::socket& socket, uint16_t port) {
  const asio::ip::udp::endpoint endpoint(asio::ip::udp::v4(), port);
  std::error_code error;
  socket.open(endpoint.protocol(), error);
  if (!error) {
    socket.bind(endpoint, error);
  }
  if (error) {
    return absl::UnavailableError(
        absl::StrCat("couldn't bind UDP port ", port, ": ", error.message()));
  }
  return absl::OkStatus();
}

absl::Status ListenAndBind(asio::ip::tcp::acceptor& acceptor,
                           asio::ip::udp::socket& socket,
                           std::span<const uint16_t> ports) {
  absl::Status status = absl::InvalidArgumentError("no port to listen on");
  for (const uint16_t port : ports) {
    status = Listen(acceptor, port);
    std::error_code ignored;
    if (status.ok()) {
      status = Bind(socket, acceptor.local_endpoint(ignored).port());
    }
    if (status.ok()) {
      return status;
    }
    acceptor.close(ignored);
    socket.close(ignored);
  }
  return status;
}

}  // namespace nanommo
