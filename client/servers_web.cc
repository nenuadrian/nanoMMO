#include <memory>
#include <string>

#include "absl/status/status.h"
#include "client/servers.h"

namespace nanommo {
namespace {

// A page can't open a socket, so there's no server to reach from one.
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
    return absl::UnavailableError("a browser has no way to the servers yet");
  }
};

}  // namespace

absl::StatusOr<std::unique_ptr<Servers>> Servers::Create(std::string, bool) {
  return std::make_unique<NoServers>();
}

}  // namespace nanommo
