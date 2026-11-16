#include <emscripten.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "client/servers.h"
#include "client/websocket_link.h"

namespace nanommo {
namespace {

// The gateway the page was opened with, as in
// nanommo_client.html?gateway=ws://localhost:8090, or nothing.
std::string PageGateway() {
  return emscripten_run_script_string(
      "new URLSearchParams(window.location.search).get('gateway') || ''");
}

// A page can't open a socket, but it can open a WebSocket, to the gateway,
// which carries each one to the login server or a world, over TLS. The worlds
// come over that one stream, as a page has no UDP either.
class GatewayServers final : public Servers {
 public:
  explicit GatewayServers(std::string gateway) : gateway_(std::move(gateway)) {}

  absl::StatusOr<protocol::LoginReply> Ask(
      const protocol::LoginRequest& request) override {
    return AskOverWebSocket(absl::StrCat(gateway_, "/login"), request);
  }

  absl::StatusOr<std::unique_ptr<Link>> Enter(
      const protocol::WorldListing& world, const std::string& token) override {
    return Open(world.host(), world.port(), token);
  }

  absl::StatusOr<std::unique_ptr<Link>> Move(
      const protocol::Move& move) override {
    return Open(move.host(), move.port(), move.token());
  }

 private:
  // The world at host and port, as the login server lists it, through the
  // gateway.
  absl::StatusOr<std::unique_ptr<Link>> Open(const std::string& host,
                                             uint32_t port,
                                             const std::string& token) {
    absl::StatusOr<std::unique_ptr<WebSocketLink>> link = WebSocketLink::Open(
        absl::StrCat(gateway_, "/world/", host, "/", port), token);
    if (!link.ok()) {
      return link.status();
    }
    return std::unique_ptr<Link>(*std::move(link));
  }

  std::string gateway_;
};

// Without a gateway, there's no server to reach from a page, so it plays
// offline, and nothing asks these.
class NoServers final : public Servers {
 public:
  absl::StatusOr<protocol::LoginReply> Ask(
      const protocol::LoginRequest&) override {
    return Unreachable();
  }

  absl::StatusOr<std::unique_ptr<Link>> Enter(const protocol::WorldListing&,
                                              const std::string&) override {
    return Unreachable();
  }

  absl::StatusOr<std::unique_ptr<Link>> Move(const protocol::Move&) override {
    return Unreachable();
  }

 private:
  static absl::Status Unreachable() {
    return absl::UnavailableError(
        "the page was opened without a gateway to the servers");
  }
};

}  // namespace

bool CanReachServers() { return !PageGateway().empty(); }

absl::StatusOr<std::unique_ptr<Servers>> Servers::Create(std::string, bool) {
  std::string gateway = PageGateway();
  if (gateway.empty()) {
    return std::make_unique<NoServers>();
  }
  return std::make_unique<GatewayServers>(std::move(gateway));
}

}  // namespace nanommo
