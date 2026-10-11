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

  explicit Tokens(Redis& redis);

  absl::StatusOr<std::string> Issue(uint32_t account_id);

  /// Returns a NotFound error for a token that's unknown, used or expired.
  absl::StatusOr<uint32_t> Redeem(const std::string& token);

 private:
  Redis& redis_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_TOKENS_H_
