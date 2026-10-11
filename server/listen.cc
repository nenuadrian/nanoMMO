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

}  // namespace nanommo
