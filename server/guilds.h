#ifndef NANOMMO_SERVER_GUILDS_H_
#define NANOMMO_SERVER_GUILDS_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "protocol/nanommo.pb.h"
#include "server/database.h"

namespace nanommo {

/// Whether a guild may be called that: 3 to 24 letters, with single spaces
/// between words, or an InvalidArgument error that says what's wrong.
absl::Status CheckGuildName(std::string_view name);

/// The guild a character is in, and its rank there.
struct Membership {
  uint32_t guild = 0;
  std::string name;
  protocol::Rank rank = protocol::RANK_MEMBER;
};

/// Guilds and who's in them, in MySQL. A guild is in one world, as its
/// characters are, and a character is in one guild at most. Unlike a player,
/// whose own world saves them, a guild changes for players other than the one
/// who acts, so it isn't in Redis: each change is a transaction.
class Guilds {
 public:
  explicit Guilds(Database& database) : database_(database) {}

  /// The guild the character is in, or a NotFound error for none.
  absl::StatusOr<Membership> Of(uint32_t player);

  /// A new guild in the world, which the character founds and leads. An
  /// InvalidArgument error says what's wrong with the name, and an
  /// AlreadyExists error that the name is taken in the world, or that the
  /// character is in a guild already.
  absl::StatusOr<Membership> Create(const std::string& world, uint32_t player,
                                    const std::string& name);

 private:
  Database& database_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_GUILDS_H_
