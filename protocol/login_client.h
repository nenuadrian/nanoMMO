#ifndef NANOMMO_PROTOCOL_LOGIN_CLIENT_H_
#define NANOMMO_PROTOCOL_LOGIN_CLIENT_H_

#include <cstdint>
#include <string>

#include "absl/status/statusor.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// Sends the request to the login server and waits for its reply, or returns
/// why it couldn't.
absl::StatusOr<protocol::LoginReply> LogIn(
    const std::string& host, uint16_t port,
    const protocol::LoginRequest& request);

}  // namespace nanommo

#endif  // NANOMMO_PROTOCOL_LOGIN_CLIENT_H_
