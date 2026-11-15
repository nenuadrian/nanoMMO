#ifndef NANOMMO_CLIENT_SERVERS_H_
#define NANOMMO_CLIENT_SERVERS_H_

#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "client/link.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// Whether this build can reach the servers at all: a browser's can't.
bool CanReachServers();

/// The login server and the worlds it lists, over the network. In a browser,
/// which gives us no sockets, each says why it can't be reached.
class Servers {
 public:
  /// The servers, with the login server at host, and the worlds sent over
  /// UDP, or TCP. Or why not, such as a certificate that won't load.
  static absl::StatusOr<std::unique_ptr<Servers>> Create(std::string host,
                                                         bool udp);

  virtual ~Servers() = default;

  /// The login server's answer to the request.
  virtual absl::StatusOr<protocol::LoginReply> Ask(
      const protocol::LoginRequest& request) = 0;

  /// Into the world, as the character the token's for.
  virtual absl::StatusOr<std::unique_ptr<Link>> Enter(
      const protocol::WorldListing& world, const std::string& token) = 0;

  /// To the world's instance its last one handed us over to.
  virtual absl::StatusOr<std::unique_ptr<Link>> Move(
      const protocol::Move& move) = 0;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_SERVERS_H_
