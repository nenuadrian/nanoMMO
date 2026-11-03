#include "server/login_server.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "protocol/framing.h"
#include "protocol/version.h"
#include "server/characters.h"
#include "server/listen.h"
#include "server/password.h"
#include "server/session.h"

namespace nanommo {

namespace {

// The account the name and password are for, or a reply that says why not:
// a wrong password, or a name that's taken or no good for a new account.
absl::StatusOr<uint32_t> Authenticate(const protocol::LoginRequest& request,
                                      Accounts& accounts,
                                      protocol::LoginReply& reply) {
  if (request.new_account()) {
    absl::Status problem = CheckUsername(request.username());
    if (problem.ok()) {
      problem = CheckPassword(request.password());
    }
    if (!problem.ok()) {
      reply.set_error(problem.message());
      return 0;
    }
    const absl::StatusOr<std::string> hash = HashPassword(request.password());
    if (!hash.ok()) {
      return hash.status();
    }
    const absl::StatusOr<uint32_t> created =
        accounts.Create(request.username(), *hash);
    if (absl::IsAlreadyExists(created.status())) {
      reply.set_error("That name is taken.");
      return 0;
    }
    return created;
  }
  const absl::StatusOr<Account> account = accounts.Find(request.username());
  if (!account.ok() && !absl::IsNotFound(account.status())) {
    return account.status();
  }
  if (!account.ok() ||
      !VerifyPassword(account->password_hash, request.password())) {
    reply.set_error("Wrong name or password.");
    return 0;
  }
  return account->id;
}

// An error the player can read, from a status a character's request got,
// or the status itself, when it's the servers' and not theirs.
absl::Status Readable(const absl::Status& status,
                      protocol::LoginReply& reply) {
  if (absl::IsInvalidArgument(status) || absl::IsAlreadyExists(status) ||
      absl::IsResourceExhausted(status) || absl::IsNotFound(status)) {
    reply.set_error(status.message());
    return absl::OkStatus();
  }
  return status;
}

}  // namespace

absl::StatusOr<protocol::LoginReply> HandleLogin(
    const protocol::LoginRequest& request, Accounts& accounts, Tokens& tokens,
    Characters& characters) {
  protocol::LoginReply reply;
  uint32_t account = 0;
  if (request.session().empty()) {
    // A client of another version would log in, then fail to make sense of
    // what the world sends it: it's told which version to get instead.
    if (!Compatible(request.version(), kVersion)) {
      reply.set_error(
          request.version().empty()
              ? absl::StrCat("This client is too old for the servers, which "
                             "are ",
                             kVersion, ". Get the new one to play.")
              : absl::StrCat("This client is ", request.version(),
                             ", and the servers are ", kVersion, ". Get the ",
                             kVersion, " client to play."));
      return reply;
    }
    // Logging in, or making an account: a session for what comes next.
    const absl::StatusOr<uint32_t> id =
        Authenticate(request, accounts, reply);
    if (!id.ok()) {
      return id.status();
    }
    if (!reply.error().empty()) {
      return reply;
    }
    account = *id;
    const absl::StatusOr<std::string> session = tokens.Open(account);
    if (!session.ok()) {
      return session.status();
    }
    reply.set_session(*session);
  } else {
    const absl::StatusOr<uint32_t> id = tokens.AccountOf(request.session());
    if (absl::IsNotFound(id.status())) {
      reply.set_error("You've been away a while. Log in again.");
      return reply;
    }
    if (!id.ok()) {
      return id.status();
    }
    account = *id;
    reply.set_session(request.session());
  }

  if (request.enter()) {
    // A token to enter the character's world as it, which the world checks
    // the character's in, once we know it's the account's.
    const absl::StatusOr<std::string> world =
        characters.WorldOf(account, request.character());
    if (world.ok()) {
      const absl::StatusOr<std::string> token =
          tokens.Issue(request.character());
      if (!token.ok()) {
        return token.status();
      }
      reply.set_token(*token);
    } else if (absl::Status unreadable = Readable(world.status(), reply);
               !unreadable.ok()) {
      return unreadable;
    }
  } else if (request.create_character()) {
    const absl::StatusOr<uint32_t> created =
        characters.Create(account, request.world(), request.name());
    if (!created.ok()) {
      if (absl::Status unreadable = Readable(created.status(), reply);
          !unreadable.ok()) {
        return unreadable;
      }
    }
  } else if (request.delete_character()) {
    if (absl::Status unreadable =
            Readable(characters.Delete(account, request.character()), reply);
        !unreadable.ok()) {
      return unreadable;
    }
  }
  // Whatever it asked, the characters as they are now come back, so the
  // client's list is the server's.
  absl::StatusOr<std::vector<protocol::CharacterListing>> listed =
      characters.List(account);
  if (!listed.ok()) {
    return listed.status();
  }
  reply.mutable_characters()->Assign(listed->begin(), listed->end());
  reply.set_ok(reply.error().empty());
  return reply;
}

absl::StatusOr<std::unique_ptr<LoginServer>> LoginServer::Create(
    asio::io_context& io, uint16_t port, asio::ssl::context& tls,
    Accounts& accounts, Tokens& tokens, Characters& characters,
    Consul& consul) {
  std::unique_ptr<LoginServer> server = absl::WrapUnique(
      new LoginServer(io, tls, accounts, tokens, characters, consul));
  const absl::Status started = server->Start(port);
  if (!started.ok()) {
    return started;
  }
  return server;
}

LoginServer::LoginServer(asio::io_context& io, asio::ssl::context& tls,
                         Accounts& accounts, Tokens& tokens,
                         Characters& characters, Consul& consul)
    : acceptor_(io),
      tls_(tls),
      accounts_(accounts),
      tokens_(tokens),
      characters_(characters),
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
            HandleLogin(request, accounts_, tokens_, characters_);
        protocol::LoginReply reply;
        if (handled.ok()) {
          reply = *std::move(handled);
        } else {
          std::cerr << "nanommo login: " << handled.status() << "\n";
          reply.set_error("The server couldn't do that. Try again later.");
        }
        // A login, as it happens: the requests that follow it go with its
        // session, without a name.
        if (request.session().empty()) {
          if (reply.ok()) {
            std::cout << "nanommo login: " << request.username()
                      << (request.new_account() ? " created an account"
                                                : " logged in")
                      << std::endl;
          } else {
            std::cout << "nanommo login: refused " << request.username()
                      << ": " << reply.error() << std::endl;
          }
        }
        // The worlds come with each answer, for the list the client shows.
        if (reply.ok()) {
          reply.mutable_worlds()->CopyFrom(ListWorlds().worlds());
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
