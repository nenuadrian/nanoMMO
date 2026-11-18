#ifndef NANOMMO_PROTOCOL_LOGIN_CLIENT_H_
#define NANOMMO_PROTOCOL_LOGIN_CLIENT_H_

#include <chrono>
#include <cstdint>
#include <string>

#include "absl/status/statusor.h"
#include "asio/ssl/context.hpp"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// How long a login waits for the login server's answer, once it has the
/// server's address: connecting, shaking hands, asking and the reply. A login
/// that takes longer is one a player's given up on. Looking the address up
/// can take longer, as asio waits for the system's lookup to end.
inline constexpr std::chrono::seconds kLoginDeadline{5};

/// Sends the request to the login server over TLS, and waits for its reply,
/// or returns why it couldn't: DeadlineExceeded when the reply didn't come
/// within the deadline, as from a login server that's hung.
absl::StatusOr<protocol::LoginReply> LogIn(
    const std::string& host, uint16_t port, asio::ssl::context& tls,
    const protocol::LoginRequest& request,
    std::chrono::milliseconds deadline = kLoginDeadline);

}  // namespace nanommo

#endif  // NANOMMO_PROTOCOL_LOGIN_CLIENT_H_
