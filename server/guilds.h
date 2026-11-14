#ifndef NANOMMO_SERVER_GUILDS_H_
#define NANOMMO_SERVER_GUILDS_H_

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "protocol/nanommo.pb.h"
#include "server/database.h"
#include "server/redis.h"

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
/// who acts, so it isn't in Redis: each change is a transaction. Only the
/// invitations, which run out, are in Redis.
class Guilds {
 public:
  /// How many characters a guild can have.
  static constexpr int kMostMembers = 100;
  /// How long an invitation waits for an answer.
  static constexpr std::chrono::seconds kInvitationLife{60};

  Guilds(Database& database, Redis& redis)
      : database_(database), redis_(redis) {}

  /// The guild the character is in, or a NotFound error for none.
  absl::StatusOr<Membership> Of(uint32_t player);

  /// A new guild in the world, which the character founds and leads. An
  /// InvalidArgument error says what's wrong with the name, and an
  /// AlreadyExists error that the name is taken in the world, or that the
  /// character is in a guild already.
  absl::StatusOr<Membership> Create(const std::string& world, uint32_t player,
                                    const std::string& name);

  /// Asks the player to join the guild of the one who asks, which an officer
  /// or the leader can do. A NotFound error says the one who asks is in no
  /// guild, a PermissionDenied one that they're a member, and AlreadyExists
  /// that a guild has asked the player, a minute ago at most, and they haven't
  /// answered. Returns the guild asked to join.
  absl::StatusOr<Membership> Invite(uint32_t from, uint32_t player);

  /// The player joins the guild that asked them, as a member. A NotFound
  /// error says nobody asked, or that it's run out, or the guild is gone,
  /// AlreadyExists that they're in a guild, and ResourceExhausted that the
  /// guild is full.
  absl::StatusOr<Membership> Accept(uint32_t player);

  /// The player says no to the guild that asked, so that another can ask.
  absl::Status Decline(uint32_t player);

 private:
  Database& database_;
  Redis& redis_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_GUILDS_H_
