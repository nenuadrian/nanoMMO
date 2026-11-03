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
#include "server/characters.h"
#include "server/consul.h"
#include "server/tokens.h"

namespace nanommo {

/// Answers a request: a login, or a new account, with a session and the
/// account's characters, and then, with that session, a new character, a
/// deleted one, or a token to enter a world as one, with the characters as
/// they are after it. A player's mistake, such as a wrong password or a name
/// that's taken, comes back as an error they can read. Returns an error status
/// when the servers it uses can't answer, such as MySQL or Redis being down.
absl::StatusOr<protocol::LoginReply> HandleLogin(
    const protocol::LoginRequest& request, Accounts& accounts, Tokens& tokens,
    Characters& characters);

class LoginServer {
 public:
  /// Starts listening on the port, or returns why it couldn't. Each client
  /// talks to it over TLS, with the context.
  static absl::StatusOr<std::unique_ptr<LoginServer>> Create(
      asio::io_context& io, uint16_t port, asio::ssl::context& tls,
      Accounts& accounts, Tokens& tokens, Characters& characters,
      Consul& consul);

 private:
  LoginServer(asio::io_context& io, asio::ssl::context& tls, Accounts& accounts,
              Tokens& tokens, Characters& characters, Consul& consul);

  absl::Status Start(uint16_t port);
  void Accept();
  protocol::LoginReply ListWorlds();

  asio::ip::tcp::acceptor acceptor_;
  asio::ssl::context& tls_;
  Accounts& accounts_;
  Tokens& tokens_;
  Characters& characters_;
  Consul& consul_;
  protocol::LoginReply worlds_;
  std::chrono::steady_clock::time_point worlds_at_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_LOGIN_SERVER_H_
