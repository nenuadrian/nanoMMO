#ifndef NANOMMO_SERVER_LISTEN_H_
#define NANOMMO_SERVER_LISTEN_H_

#include <cstdint>

#include "absl/status/status.h"
#include "asio/ip/tcp.hpp"

namespace nanommo {

/// Opens the acceptor on the port, on all addresses, and starts listening.
absl::Status Listen(asio::ip::tcp::acceptor& acceptor, uint16_t port);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_LISTEN_H_
