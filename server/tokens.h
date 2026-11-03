#ifndef NANOMMO_SERVER_TOKENS_H_
#define NANOMMO_SERVER_TOKENS_H_

#include <chrono>
#include <cstdint>
#include <string>

#include "absl/status/statusor.h"
#include "server/redis.h"

namespace nanommo {

class Tokens {
 public:
  static constexpr std::chrono::seconds kLifetime{30};
  /// How long a login's session lasts after its last request.
  static constexpr std::chrono::minutes kSessionLifetime{30};

  explicit Tokens(Redis& redis);

  /// A token to enter a world with, once, as the player with the id: a
  /// character, from the login server, or one moving to another instance.
  absl::StatusOr<std::string> Issue(uint32_t player_id);

  /// Returns a NotFound error for a token that's unknown, used or expired.
  absl::StatusOr<uint32_t> Redeem(const std::string& token);

  /// A session for the account, which its next requests to the login server
  /// go with in place of the name and the password.
  absl::StatusOr<std::string> Open(uint32_t account_id);

  /// The account the session is for, which keeps it going for another
  /// kSessionLifetime, or a NotFound error once it's run out.
  absl::StatusOr<uint32_t> AccountOf(const std::string& session);

 private:
  Redis& redis_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_TOKENS_H_
