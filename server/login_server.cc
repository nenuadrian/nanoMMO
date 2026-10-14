#include "server/login_server.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "protocol/framing.h"
#include "server/listen.h"
#include "server/password.h"
#include "server/session.h"

namespace nanommo {

absl::StatusOr<protocol::LoginReply> HandleLogin(
    const protocol::LoginRequest& request, Accounts& accounts, Tokens& tokens) {
  protocol::LoginReply reply;
  uint32_t id = 0;
  if (request.new_account()) {
    absl::Status problem = CheckUsername(request.username());
    if (problem.ok()) {
      problem = CheckPassword(request.password());
    }
    if (!problem.ok()) {
      reply.set_error(problem.message());
      return reply;
    }
    const absl::StatusOr<std::string> hash = HashPassword(request.password());
    if (!hash.ok()) {
      return hash.status();
    }
    const absl::StatusOr<uint32_t> created =
        accounts.Create(request.username(), *hash);
    if (absl::IsAlreadyExists(created.status())) {
      reply.set_error("That name is taken.");
      return reply;
    }
    if (!created.ok()) {
      return created.status();
    }
    id = *created;
  } else {
    const absl::StatusOr<Account> account = accounts.Find(request.username());
    if (!account.ok() && !absl::IsNotFound(account.status())) {
      return account.status();
    }
    if (!account.ok() ||
        !VerifyPassword(account->password_hash, request.password())) {
      reply.set_error("Wrong name or password.");
      return reply;
    }
    id = account->id;
  }
  const absl::StatusOr<std::string> token = tokens.Issue(id);
  if (!token.ok()) {
    return token.status();
  }
  reply.set_ok(true);
  reply.set_token(*token);
  return reply;
}

absl::StatusOr<std::unique_ptr<LoginServer>> LoginServer::Create(
    asio::io_context& io, uint16_t port, asio::ssl::context& tls,
    Accounts& accounts, Tokens& tokens, Consul& consul) {
  std::unique_ptr<LoginServer> server =
      absl::WrapUnique(new LoginServer(io, tls, accounts, tokens, consul));
  const absl::Status started = server->Start(port);
  if (!started.ok()) {
    return started;
  }
  return server;
}

LoginServer::LoginServer(asio::io_context& io, asio::ssl::context& tls,
                         Accounts& accounts, Tokens& tokens, Consul& consul)
    : acceptor_(io),
      tls_(tls),
      accounts_(accounts),
      tokens_(tokens),
      consul_(consul) {}

absl::Status LoginServer::Start(uint16_t port) {
  const absl::Status listening = Listen(acceptor_, port);
  if (!listening.ok()) {
    return listening;
  }
  Accept();
  return absl::OkStatus();
}

void LoginServer::Accept() {
  acceptor_.async_accept([this](std::error_code error,
                                asio::ip::tcp::socket socket) {
    if (!error) {
      const auto on_message = [this](Session& session,
                                     const std::string& bytes) {
        protocol::LoginRequest request;
        if (!request.ParseFromString(bytes)) {
          session.Close();
          return;
        }
        if (request.list_worlds()) {
          session.Send(Frame(ListWorlds()));
          return;
        }
        absl::StatusOr<protocol::LoginReply> handled =
            HandleLogin(request, accounts_, tokens_);
        protocol::LoginReply reply;
        if (handled.ok()) {
          reply = *std::move(handled);
        } else {
          std::cerr << "nanommo login: " << handled.status() << "\n";
          reply.set_error("The server couldn't log you in. Try again later.");
        }
        if (reply.ok()) {
          std::cout << "nanommo login: " << request.username()
                    << (request.new_account() ? " created an account"
                                              : " logged in")
                    << std::endl;
        } else {
          std::cout << "nanommo login: refused " << request.username() << ": "
                    << reply.error() << std::endl;
        }
        session.Send(Frame(reply));
      };
      const auto on_close = [] {};
      std::make_shared<Session>(std::move(socket), tls_, on_message, on_close)
          ->Start();
    }
    Accept();
  });
}

protocol::LoginReply LoginServer::ListWorlds() {
  // A list from the last second is new enough, and a crowd of clients costs
  // Consul a request a second, while this thread waits for its answer.
  const auto now = std::chrono::steady_clock::now();
  if (worlds_.ok() && now - worlds_at_ < std::chrono::seconds(1)) {
    return worlds_;
  }
  protocol::LoginReply reply;
  const absl::StatusOr<std::vector<protocol::WorldListing>> worlds =
      consul_.Worlds();
  if (!worlds.ok()) {
    std::cerr << "nanommo login: " << worlds.status() << "\n";
    reply.set_error("The server couldn't list the worlds. Try again later.");
    return reply;
  }
  reply.set_ok(true);
  reply.mutable_worlds()->Assign(worlds->begin(), worlds->end());
  worlds_ = reply;
  worlds_at_ = now;
  return reply;
}

}  // namespace nanommo
