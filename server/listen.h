#ifndef NANOMMO_SERVER_LISTEN_H_
#define NANOMMO_SERVER_LISTEN_H_

#include <cstdint>
#include <span>

#include "absl/status/status.h"
#include "asio/ip/tcp.hpp"
#include "asio/ip/udp.hpp"

namespace nanommo {

/// Opens the acceptor on the port, on all addresses, and starts listening.
absl::Status Listen(asio::ip::tcp::acceptor& acceptor, uint16_t port);

/// Opens the socket on the port, on all addresses, for datagrams.
absl::Status Bind(asio::ip::udp::socket& socket, uint16_t port);

/// Listens on TCP and binds UDP on one port number, the first of `ports` that
/// is free for both, where 0 is a port the system picks for TCP. What it opened
/// for a port that didn't work, it closes again, and with none that did, it
/// says why the last one didn't.
absl::Status ListenAndBind(asio::ip::tcp::acceptor& acceptor,
                           asio::ip::udp::socket& socket,
                           std::span<const uint16_t> ports);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_LISTEN_H_
