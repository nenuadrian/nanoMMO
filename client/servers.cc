#include "client/servers.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "asio/ssl/context.hpp"
#include "client/connection.h"
#include "protocol/login_client.h"
#include "protocol/settings.h"
#include "protocol/tls.h"

namespace nanommo {
namespace {

constexpr uint16_t kLoginPort = 7778;

// Whether host is the machine it's said on.
bool ThisMachine(const std::string& host) {
  return host == "localhost" || host == "127.0.0.1" || host == "::1";
}

class NetworkServers final : public Servers {
 public:
  NetworkServers(std::string host, bool udp, asio::ssl::context tls)
      : host_(std::move(host)), udp_(udp), tls_(std::move(tls)) {}

  absl::StatusOr<protocol::LoginReply> Ask(
      const protocol::LoginRequest& request) override {
    return LogIn(host_, kLoginPort, tls_, request);
  }

  absl::StatusOr<std::unique_ptr<Link>> Enter(
      const protocol::WorldListing& world, const std::string& token) override {
    absl::StatusOr<std::unique_ptr<Connection>> connection = Connection::Create(
        world.host(), static_cast<uint16_t>(world.port()), tls_, token, udp_);
    if (!connection.ok()) {
      std::string why(connection.status().message());
      // A world started without NANOMMO_PUBLIC_HOST gives localhost, which
      // from another machine is that machine, not the world's.
      if (ThisMachine(world.host()) && !ThisMachine(host_)) {
        absl::StrAppend(&why, ". ", world.name(), " says it's at ",
                        world.host(),
                        ": start its server with NANOMMO_PUBLIC_HOST=", host_);
      }
      return absl::UnavailableError(why);
    }
    return std::unique_ptr<Link>(*std::move(connection));
  }

  absl::StatusOr<std::unique_ptr<Link>> Move(
      const protocol::Move& move) override {
    absl::StatusOr<std::unique_ptr<Connection>> connection =
        Connection::Create(move.host(), static_cast<uint16_t>(move.port()),
                           tls_, move.token(), udp_);
    if (!connection.ok()) {
      return connection.status();
    }
    return std::unique_ptr<Link>(*std::move(connection));
  }

 private:
  std::string host_;
  bool udp_;
  asio::ssl::context tls_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<Servers>> Servers::Create(std::string host,
                                                         bool udp) {
  absl::StatusOr<asio::ssl::context> tls =
      ClientTls(FileSetting("NANOMMO_TLS_CA", "tls/ca.crt"));
  if (!tls.ok()) {
    return tls.status();
  }
  return std::make_unique<NetworkServers>(std::move(host), udp,
                                          *std::move(tls));
}

}  // namespace nanommo
