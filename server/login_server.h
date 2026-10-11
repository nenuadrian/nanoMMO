#ifndef NANOMMO_SERVER_LOGIN_SERVER_H_
#define NANOMMO_SERVER_LOGIN_SERVER_H_

#include <cstdint>
#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "protocol/nanommo.pb.h"
#include "server/accounts.h"
#include "server/tokens.h"

namespace nanommo {

/// Answers a request with a token, or with an error the player can read, such
/// as a wrong password. Returns an error status when the servers it uses can't
/// answer, such as MySQL or Redis being down.
absl::StatusOr<protocol::LoginReply> HandleLogin(
    const protocol::LoginRequest& request, Accounts& accounts, Tokens& tokens);

class LoginServer {
 public:
  /// Starts listening on the port, or returns why it couldn't.
  static absl::StatusOr<std::unique_ptr<LoginServer>> Create(
      asio::io_context& io, uint16_t port, Accounts& accounts, Tokens& tokens);

 private:
  LoginServer(asio::io_context& io, Accounts& accounts, Tokens& tokens);

  absl::Status Start(uint16_t port);
  void Accept();

  asio::ip::tcp::acceptor acceptor_;
  Accounts& accounts_;
  Tokens& tokens_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_LOGIN_SERVER_H_
