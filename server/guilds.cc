#include "server/guilds.h"

#include <cstddef>
#include <string>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "server/levels.h"

namespace nanommo {

namespace {

// Each player has one invitation at a time, by their character's id, which
// holds the guild's.
std::string InvitationOf(uint32_t player) {
  return absl::StrCat("guild_invite:", player);
}

// A member's rank in the guild, with their row locked, or NotFound when they
// aren't in it.
absl::StatusOr<protocol::Rank> RankIn(Database& database, uint32_t guild,
                                      uint32_t player) {
  const absl::StatusOr<Result> member = database.Run(
      "SELECT grade FROM guild_members WHERE player = ? AND guild = ? "
      "FOR UPDATE",
      {absl::StrCat(player), absl::StrCat(guild)});
  if (!member.ok()) {
    return member.status();
  }
  int grade = 0;
  if (member->rows.empty()) {
    return absl::NotFoundError("They're not in your guild.");
  }
  if (!absl::SimpleAtoi(member->rows.front()[0], &grade) ||
      !protocol::Rank_IsValid(grade)) {
    return absl::InternalError("a grade that isn't a rank");
  }
  return static_cast<protocol::Rank>(grade);
}

// The guild of a player who's changing something in it, in a transaction:
// its row locked first, as every change to a guild locks it, so changes to
// one guild happen one at a time, then the player's own, which someone may
// have changed before we got the guild's.
absl::StatusOr<Membership> LockFor(Database& database, uint32_t player) {
  Membership membership;
  const absl::StatusOr<Result> in =
      database.Run("SELECT guild FROM guild_members WHERE player = ?",
                   {absl::StrCat(player)});
  if (!in.ok()) {
    return in.status();
  }
  if (in->rows.empty() ||
      !absl::SimpleAtoi(in->rows.front()[0], &membership.guild)) {
    return absl::NotFoundError("You're in no guild.");
  }
  const absl::StatusOr<Result> guild =
      database.Run("SELECT name FROM guilds WHERE id = ? FOR UPDATE",
                   {absl::StrCat(membership.guild)});
  if (!guild.ok()) {
    return guild.status();
  }
  const absl::StatusOr<protocol::Rank> rank =
      RankIn(database, membership.guild, player);
  if (guild->rows.empty() || absl::IsNotFound(rank.status())) {
    return absl::NotFoundError("You're in no guild.");
  }
  if (!rank.ok()) {
    return rank.status();
  }
  membership.name = guild->rows.front()[0];
  membership.rank = *rank;
  return membership;
}

}  // namespace

absl::Status CheckGuildName(std::string_view name) {
  if (name.size() < 3 || name.size() > 24) {
    return absl::InvalidArgumentError(
        "Guild names are 3 to 24 characters long.");
  }
  // Words of letters, one space apart, so a name starts and ends with a
  // letter, and two names can't differ by their spaces alone.
  for (size_t i = 0; i < name.size(); ++i) {
    const bool between_words =
        name[i] == ' ' && i > 0 && i + 1 < name.size() && name[i - 1] != ' ';
    if (!absl::ascii_isalpha(static_cast<unsigned char>(name[i])) &&
        !between_words) {
      return absl::InvalidArgumentError(
          "Guild names are letters, with one space between words.");
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<Membership> Guilds::Of(uint32_t player) {
  const absl::StatusOr<Result> rows = database_.Run(
      "SELECT guilds.id, guilds.name, guild_members.grade "
      "FROM guild_members JOIN guilds ON guilds.id = guild_members.guild "
      "WHERE guild_members.player = ?",
      {absl::StrCat(player)});
  if (!rows.ok()) {
    return rows.status();
  }
  if (rows->rows.empty()) {
    return absl::NotFoundError("You're in no guild.");
  }
  const std::vector<std::string>& row = rows->rows.front();
  Membership membership;
  int grade = 0;
  if (!absl::SimpleAtoi(row[0], &membership.guild) ||
      !absl::SimpleAtoi(row[2], &grade) || !protocol::Rank_IsValid(grade)) {
    return absl::InternalError("a guild member that isn't numbers");
  }
  membership.name = row[1];
  membership.rank = static_cast<protocol::Rank>(grade);
  return membership;
}

absl::StatusOr<Membership> Guilds::Create(const std::string& world,
                                          uint32_t player,
                                          const std::string& name) {
  if (const absl::Status checked = CheckGuildName(name); !checked.ok()) {
    return checked;
  }
  Membership membership{.name = name, .rank = protocol::RANK_LEADER};
  // The guild and its leader go in together, so when the character is in a
  // guild already, the new guild goes too, and its name stays free.
  const absl::Status created = database_.Transaction([&]() -> absl::Status {
    const absl::StatusOr<Result> guild = database_.Run(
        "INSERT INTO guilds (world, name) VALUES (?, ?)", {world, name});
    if (absl::IsAlreadyExists(guild.status())) {
      return absl::AlreadyExistsError(
          absl::StrCat("There's a guild called that in ", world, " already."));
    }
    if (!guild.ok()) {
      return guild.status();
    }
    membership.guild = static_cast<uint32_t>(guild->insert_id);
    const absl::StatusOr<Result> leader = database_.Run(
        "INSERT INTO guild_members (player, guild, grade) VALUES (?, ?, ?)",
        {absl::StrCat(player), absl::StrCat(membership.guild),
         absl::StrCat(static_cast<int>(protocol::RANK_LEADER))});
    if (absl::IsAlreadyExists(leader.status())) {
      return absl::AlreadyExistsError("You're in a guild already.");
    }
    return leader.status();
  });
  if (!created.ok()) {
    return created;
  }
  return membership;
}

absl::StatusOr<Membership> Guilds::Invite(uint32_t from, uint32_t player) {
  absl::StatusOr<Membership> asking = Of(from);
  if (!asking.ok()) {
    return asking.status();
  }
  if (asking->rank == protocol::RANK_MEMBER) {
    return absl::PermissionDeniedError(
        "Only officers and the leader can ask someone in.");
  }
  // NX sets it only when there's none, so of two asking at once, from any
  // instance, one does, and the invitation runs out by itself.
  const absl::StatusOr<std::string> asked =
      redis_.Command({"SET", InvitationOf(player), absl::StrCat(asking->guild),
                      "EX", absl::StrCat(kInvitationLife.count()), "NX"});
  if (absl::IsNotFound(asked.status())) {
    return absl::AlreadyExistsError("They've been asked to join a guild.");
  }
  if (!asked.ok()) {
    return asked.status();
  }
  return asking;
}

absl::StatusOr<Membership> Guilds::Accept(uint32_t player) {
  // Read, and not taken yet, so that a MySQL that fails for a moment leaves
  // it to try again. Two at once can't both join, as a character is in one
  // guild at most.
  const absl::StatusOr<std::string> invitation =
      redis_.Command({"GET", InvitationOf(player)});
  if (absl::IsNotFound(invitation.status())) {
    return absl::NotFoundError("Nobody's asked you, or it's run out.");
  }
  if (!invitation.ok()) {
    return invitation.status();
  }
  Membership membership;
  if (!absl::SimpleAtoi(*invitation, &membership.guild)) {
    return absl::InternalError("an invitation that isn't a number");
  }
  const absl::Status joined = database_.Transaction([&]() -> absl::Status {
    // Locking the guild's row first, as every change to a guild does, so
    // that changes to one guild happen one at a time, and it can't end or
    // fill up while we join.
    const absl::StatusOr<Result> guild =
        database_.Run("SELECT name FROM guilds WHERE id = ? FOR UPDATE",
                      {absl::StrCat(membership.guild)});
    if (!guild.ok()) {
      return guild.status();
    }
    if (guild->rows.empty()) {
      return absl::NotFoundError("That guild is gone.");
    }
    membership.name = guild->rows.front()[0];
    const absl::StatusOr<Result> members =
        database_.Run("SELECT COUNT(*) FROM guild_members WHERE guild = ?",
                      {absl::StrCat(membership.guild)});
    if (!members.ok()) {
      return members.status();
    }
    int count = 0;
    if (!absl::SimpleAtoi(members->rows.front()[0], &count)) {
      return absl::InternalError("a count that isn't a number");
    }
    if (count >= kMostMembers) {
      return absl::ResourceExhaustedError(
          absl::StrCat(membership.name, " is full."));
    }
    const absl::StatusOr<Result> member =
        database_.Run("INSERT INTO guild_members (player, guild) VALUES (?, ?)",
                      {absl::StrCat(player), absl::StrCat(membership.guild)});
    if (absl::IsAlreadyExists(member.status())) {
      return absl::AlreadyExistsError("You're in a guild already.");
    }
    return member.status();
  });
  // Joined, or never will be with this one, it's spent. If deleting it fails,
  // it runs out by itself.
  if (!absl::IsAborted(joined) && !absl::IsUnavailable(joined)) {
    redis_.Command({"DEL", InvitationOf(player)}).status().IgnoreError();
  }
  if (!joined.ok()) {
    return joined;
  }
  return membership;
}

absl::StatusOr<Changed> Guilds::SetRank(uint32_t from, uint32_t player,
                                        protocol::Rank rank) {
  if (rank == protocol::RANK_LEADER || from == player) {
    return absl::InvalidArgumentError(
        "The leader makes members officers, and officers members.");
  }
  Changed changed;
  const absl::Status done = database_.Transaction([&]() -> absl::Status {
    const absl::StatusOr<Membership> leading = LockFor(database_, from);
    if (!leading.ok()) {
      return leading.status();
    }
    if (leading->rank != protocol::RANK_LEADER) {
      return absl::PermissionDeniedError("Only the leader changes ranks.");
    }
    changed.guild = leading->guild;
    changed.guild_name = leading->name;
    const absl::StatusOr<protocol::Rank> now =
        RankIn(database_, leading->guild, player);
    if (!now.ok()) {
      return now.status();
    }
    const absl::StatusOr<Result> named = database_.Run(
        "SELECT name FROM players WHERE id = ?", {absl::StrCat(player)});
    if (!named.ok()) {
      return named.status();
    }
    changed.player_name = named->rows.empty() ? "" : named->rows.front()[0];
    return database_
        .Run("UPDATE guild_members SET grade = ? WHERE player = ?",
             {absl::StrCat(static_cast<int>(rank)), absl::StrCat(player)})
        .status();
  });
  if (!done.ok()) {
    return done;
  }
  return changed;
}

absl::StatusOr<Changed> Guilds::Kick(uint32_t from, uint32_t player) {
  if (from == player) {
    return absl::InvalidArgumentError("To go, leave the guild.");
  }
  Changed changed;
  const absl::Status done = database_.Transaction([&]() -> absl::Status {
    const absl::StatusOr<Membership> acting = LockFor(database_, from);
    if (!acting.ok()) {
      return acting.status();
    }
    const absl::StatusOr<protocol::Rank> theirs =
        RankIn(database_, acting->guild, player);
    if (!theirs.ok()) {
      return theirs.status();
    }
    if (acting->rank <= *theirs) {
      return absl::PermissionDeniedError(
          "The leader removes officers and members, and officers members.");
    }
    changed.guild = acting->guild;
    changed.guild_name = acting->name;
    const absl::StatusOr<Result> named = database_.Run(
        "SELECT name FROM players WHERE id = ?", {absl::StrCat(player)});
    if (!named.ok()) {
      return named.status();
    }
    changed.player_name = named->rows.empty() ? "" : named->rows.front()[0];
    return database_
        .Run("DELETE FROM guild_members WHERE player = ?",
             {absl::StrCat(player)})
        .status();
  });
  if (!done.ok()) {
    return done;
  }
  return changed;
}

absl::StatusOr<std::vector<protocol::GuildMember>> Guilds::Roster(
    uint32_t guild) {
  const absl::StatusOr<Result> rows = database_.Run(
      "SELECT guild_members.player, players.name, guild_members.grade, "
      "players.experience FROM guild_members "
      "JOIN players ON players.id = guild_members.player "
      "WHERE guild_members.guild = ? "
      "ORDER BY guild_members.grade DESC, players.name",
      {absl::StrCat(guild)});
  if (!rows.ok()) {
    return rows.status();
  }
  std::vector<protocol::GuildMember> roster;
  roster.reserve(rows->rows.size());
  for (const std::vector<std::string>& row : rows->rows) {
    uint32_t id = 0;
    int grade = 0;
    uint32_t experience = 0;
    if (!absl::SimpleAtoi(row[0], &id) || !absl::SimpleAtoi(row[2], &grade) ||
        !absl::SimpleAtoi(row[3], &experience) ||
        !protocol::Rank_IsValid(grade)) {
      return absl::InternalError("a guild member that isn't numbers");
    }
    protocol::GuildMember& member = roster.emplace_back();
    member.set_id(id);
    member.set_name(row[1]);
    member.set_rank(static_cast<protocol::Rank>(grade));
    member.set_level(static_cast<uint32_t>(LevelFor(experience).number));
  }
  return roster;
}

absl::Status Guilds::Decline(uint32_t player) {
  return redis_.Command({"DEL", InvitationOf(player)}).status();
}

absl::StatusOr<Left> Guilds::Leave(uint32_t player) {
  absl::StatusOr<Left> left;
  const absl::Status done = database_.Transaction([&]() -> absl::Status {
    left = LeaveGuild(database_, player);
    return left.status();
  });
  if (!done.ok()) {
    return done;
  }
  return left;
}

absl::StatusOr<Left> LeaveGuild(Database& database, uint32_t player) {
  const absl::StatusOr<Membership> leaving = LockFor(database, player);
  if (!leaving.ok()) {
    return leaving.status();
  }
  Left left;
  left.guild = leaving->guild;
  left.name = leaving->name;
  const absl::StatusOr<Result> gone = database.Run(
      "DELETE FROM guild_members WHERE player = ?", {absl::StrCat(player)});
  if (!gone.ok()) {
    return gone.status();
  }
  if (leaving->rank != protocol::RANK_LEADER) {
    return left;
  }
  // The highest rank left, and of those, who joined first.
  const absl::StatusOr<Result> next = database.Run(
      "SELECT guild_members.player, players.name FROM guild_members "
      "JOIN players ON players.id = guild_members.player "
      "WHERE guild_members.guild = ? "
      "ORDER BY guild_members.grade DESC, guild_members.joined_at, "
      "guild_members.player LIMIT 1 FOR UPDATE",
      {absl::StrCat(left.guild)});
  if (!next.ok()) {
    return next.status();
  }
  if (next->rows.empty()) {
    left.ended = true;
    const absl::StatusOr<Result> ended = database.Run(
        "DELETE FROM guilds WHERE id = ?", {absl::StrCat(left.guild)});
    if (!ended.ok()) {
      return ended.status();
    }
    return left;
  }
  if (!absl::SimpleAtoi(next->rows.front()[0], &left.successor)) {
    return absl::InternalError("a member that isn't a number");
  }
  left.successor_name = next->rows.front()[1];
  const absl::StatusOr<Result> promoted =
      database.Run("UPDATE guild_members SET grade = ? WHERE player = ?",
                   {absl::StrCat(static_cast<int>(protocol::RANK_LEADER)),
                    absl::StrCat(left.successor)});
  if (!promoted.ok()) {
    return promoted.status();
  }
  return left;
}

}  // namespace nanommo
