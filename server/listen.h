#ifndef NANOMMO_SERVER_LISTEN_H_
#define NANOMMO_SERVER_LISTEN_H_

#include <cstdint>

#include "absl/status/status.h"
#include "asio/ip/tcp.hpp"
#include "asio/ip/udp.hpp"

namespace nanommo {

/// Opens the acceptor on the port, on all addresses, and starts listening.
absl::Status Listen(asio::ip::tcp::acceptor& acceptor, uint16_t port);

/// Opens the socket on the port, on all addresses, for datagrams.
absl::Status Bind(asio::ip::udp::socket& socket, uint16_t port);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_LISTEN_H_
