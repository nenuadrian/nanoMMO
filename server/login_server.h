#ifndef NANOMMO_SERVER_LOGIN_SERVER_H_
#define NANOMMO_SERVER_LOGIN_SERVER_H_

#include <chrono>
#include <cstdint>
#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/ssl/context.hpp"
#include "protocol/nanommo.pb.h"
#include "server/accounts.h"
#include "server/consul.h"
#include "server/tokens.h"

namespace nanommo {

/// Answers a request with a token, or with an error the player can read, such
/// as a wrong password. Returns an error status when the servers it uses can't
/// answer, such as MySQL or Redis being down.
absl::StatusOr<protocol::LoginReply> HandleLogin(
    const protocol::LoginRequest& request, Accounts& accounts, Tokens& tokens);

class LoginServer {
 public:
  /// Starts listening on the port, or returns why it couldn't. Each client
  /// talks to it over TLS, with the context.
  static absl::StatusOr<std::unique_ptr<LoginServer>> Create(
      asio::io_context& io, uint16_t port, asio::ssl::context& tls,
      Accounts& accounts, Tokens& tokens, Consul& consul);

 private:
  LoginServer(asio::io_context& io, asio::ssl::context& tls, Accounts& accounts,
              Tokens& tokens, Consul& consul);

  absl::Status Start(uint16_t port);
  void Accept();
  protocol::LoginReply ListWorlds();

  asio::ip::tcp::acceptor acceptor_;
  asio::ssl::context& tls_;
  Accounts& accounts_;
  Tokens& tokens_;
  Consul& consul_;
  protocol::LoginReply worlds_;
  std::chrono::steady_clock::time_point worlds_at_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_LOGIN_SERVER_H_
