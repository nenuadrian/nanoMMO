#ifndef NANOMMO_SERVER_GUILDS_H_
#define NANOMMO_SERVER_GUILDS_H_

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

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

/// What leaving a guild did.
struct Left {
  uint32_t guild = 0;
  std::string name;
  /// Who leads it now, when the one who left led it, and someone's still in
  /// it, by their character's id and name.
  uint32_t successor = 0;
  std::string successor_name;
  /// Whether they were the last one in it, so it's gone.
  bool ended = false;
};

/// What a change to another member of a guild did, for the news.
struct Changed {
  uint32_t guild = 0;
  std::string guild_name;
  /// The member it changed, by their name.
  std::string player_name;
};

/// The character leaves their guild. When they led it, the one who's been in
/// it longest leads it now, an officer before a member, and the last one out
/// ends it. A NotFound error says they're in none. It runs in a transaction
/// the caller has begun: Leave's own, or a character's deletion, which calls
/// it too, so that a guild never goes without a leader, and only if the
/// character goes.
absl::StatusOr<Left> LeaveGuild(Database& database, uint32_t player);

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

  /// The player leaves their guild, as LeaveGuild says, in a transaction.
  absl::StatusOr<Left> Leave(uint32_t player);

  /// The leader makes a member an officer, or an officer a member again. A
  /// PermissionDenied error says the one who asks doesn't lead, NotFound that
  /// the player isn't in their guild, and InvalidArgument that the rank isn't
  /// one they can give.
  absl::StatusOr<Changed> SetRank(uint32_t from, uint32_t player,
                                  protocol::Rank rank);

  /// Takes a player out of the guild of the one who asks, whose rank has to
  /// be above theirs: the leader removes officers and members, and officers
  /// members. A PermissionDenied error says it isn't, NotFound that the
  /// player isn't in their guild.
  absl::StatusOr<Changed> Kick(uint32_t from, uint32_t player);

  /// The leader ends their guild, for everyone in it, and its name is free
  /// again. A PermissionDenied error says the one who asks doesn't lead it.
  /// Returns the guild that ended.
  absl::StatusOr<Membership> Disband(uint32_t from);

  /// Who's in the guild, the highest rank first, and by name, as MySQL has
  /// them: their levels are a few seconds behind a world's, and none of them
  /// online, which only a world knows.
  absl::StatusOr<std::vector<protocol::GuildMember>> Roster(uint32_t guild);

 private:
  Database& database_;
  Redis& redis_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_GUILDS_H_
