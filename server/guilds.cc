#include "server/guilds.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
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

// A guild's ask to war another, by both their ids, which holds who asked.
std::string WarAskOf(const std::string& world, uint32_t guild, uint32_t enemy) {
  return absl::StrCat("war_ask:", world, ":", guild, ":", enemy);
}

// A FailedPrecondition error, in words for the leader of ours, when either
// guild is in a war that hasn't ended, as a guild is in one at a time.
absl::Status NeitherAtWar(Database& database, uint32_t ours, uint32_t theirs,
                          std::string_view their_name) {
  const std::string a = absl::StrCat(ours);
  const std::string b = absl::StrCat(theirs);
  const absl::StatusOr<Result> wars = database.Run(
      "SELECT challenger, challenged FROM guild_wars WHERE ends_at > NOW() "
      "AND (challenger IN (?, ?) OR challenged IN (?, ?))",
      {a, b, a, b});
  if (!wars.ok()) {
    return wars.status();
  }
  for (const std::vector<std::string>& war : wars->rows) {
    if (war[0] == a || war[1] == a) {
      return absl::FailedPreconditionError("Your guild is at war already.");
    }
  }
  if (!wars->rows.empty()) {
    return absl::FailedPreconditionError(
        absl::StrCat(their_name, " is at war already."));
  }
  return absl::OkStatus();
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

absl::StatusOr<Membership> Guilds::Disband(uint32_t from) {
  Membership ended;
  const absl::Status done = database_.Transaction([&]() -> absl::Status {
    const absl::StatusOr<Membership> leading = LockFor(database_, from);
    if (!leading.ok()) {
      return leading.status();
    }
    if (leading->rank != protocol::RANK_LEADER) {
      return absl::PermissionDeniedError("Only the leader disbands the guild.");
    }
    ended = *leading;
    // Its members' rows go with it.
    return database_
        .Run("DELETE FROM guilds WHERE id = ?", {absl::StrCat(ended.guild)})
        .status();
  });
  if (!done.ok()) {
    return done;
  }
  return ended;
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

absl::StatusOr<War> Guilds::AskToWar(const std::string& world, uint32_t from,
                                     const std::string& name) {
  const absl::StatusOr<Membership> asking = Of(from);
  if (!asking.ok()) {
    return asking.status();
  }
  if (asking->rank != protocol::RANK_LEADER) {
    return absl::PermissionDeniedError(
        "Only the leader takes the guild to war.");
  }
  // MySQL compares the name without its case.
  const absl::StatusOr<Result> found =
      database_.Run("SELECT id, name FROM guilds WHERE world = ? AND name = ?",
                    {world, name});
  if (!found.ok()) {
    return found.status();
  }
  if (found->rows.empty()) {
    return absl::NotFoundError(
        absl::StrCat("There's no guild called ", name, " in ", world, "."));
  }
  War war{.challenger = asking->guild, .challenger_name = asking->name};
  if (!absl::SimpleAtoi(found->rows.front()[0], &war.challenged)) {
    return absl::InternalError("a guild that isn't a number");
  }
  war.challenged_name = found->rows.front()[1];
  if (war.challenged == war.challenger) {
    return absl::InvalidArgumentError(
        "Your guild can't go to war with itself.");
  }
  // Asking back the guild that asked is a yes, as in a duel, and they're the
  // challenger.
  const absl::StatusOr<std::string> asked =
      redis_.Command({"GET", WarAskOf(world, war.challenged, war.challenger)});
  if (asked.ok()) {
    std::swap(war.challenger, war.challenged);
    std::swap(war.challenger_name, war.challenged_name);
    return StartWar(world, from, std::move(war));
  }
  if (!absl::IsNotFound(asked.status())) {
    return asked.status();
  }
  if (const absl::Status free = NeitherAtWar(
          database_, war.challenger, war.challenged, war.challenged_name);
      !free.ok()) {
    return free;
  }
  // NX sets it only when there's none, so a guild asks another once until
  // it's answered, and the ask runs out by itself.
  const absl::StatusOr<std::string> set = redis_.Command(
      {"SET", WarAskOf(world, war.challenger, war.challenged),
       absl::StrCat(from), "EX", absl::StrCat(kWarAskLife.count()), "NX"});
  if (absl::IsNotFound(set.status())) {
    return absl::AlreadyExistsError(
        absl::StrCat("You've asked ", war.challenged_name, " already."));
  }
  if (!set.ok()) {
    return set.status();
  }
  return war;
}

absl::StatusOr<War> Guilds::StartWar(const std::string& world, uint32_t from,
                                     War war) {
  const absl::Status started = database_.Transaction([&]() -> absl::Status {
    // Both guilds' rows, the lower id first, before any member's. Two leaders
    // saying yes at once, each to the other's ask, would otherwise each lock
    // their own and wait for the other's.
    for (const uint32_t guild : {std::min(war.challenger, war.challenged),
                                 std::max(war.challenger, war.challenged)}) {
      const absl::StatusOr<Result> locked =
          database_.Run("SELECT id FROM guilds WHERE id = ? FOR UPDATE",
                        {absl::StrCat(guild)});
      if (!locked.ok()) {
        return locked.status();
      }
      if (locked->rows.empty()) {
        return absl::NotFoundError("That guild is gone.");
      }
    }
    // They may have stopped leading it since we asked.
    const absl::StatusOr<protocol::Rank> rank =
        RankIn(database_, war.challenged, from);
    if (absl::IsNotFound(rank.status())) {
      return absl::NotFoundError("You're in no guild.");
    }
    if (!rank.ok()) {
      return rank.status();
    }
    if (*rank != protocol::RANK_LEADER) {
      return absl::PermissionDeniedError(
          "Only the leader takes the guild to war.");
    }
    if (const absl::Status free = NeitherAtWar(
            database_, war.challenged, war.challenger, war.challenger_name);
        !free.ok()) {
      return free;
    }
    const absl::StatusOr<Result> inserted = database_.Run(
        "INSERT INTO guild_wars (world, challenger, challenged, ends_at) "
        "VALUES (?, ?, ?, NOW() + INTERVAL ? SECOND)",
        {world, absl::StrCat(war.challenger), absl::StrCat(war.challenged),
         absl::StrCat(std::chrono::seconds(kWarLength).count())});
    if (!inserted.ok()) {
      return inserted.status();
    }
    war.id = static_cast<uint32_t>(inserted->insert_id);
    return absl::OkStatus();
  });
  // Started, or never will be from this ask, it's spent. If deleting it fails,
  // it runs out by itself.
  if (!absl::IsAborted(started) && !absl::IsUnavailable(started)) {
    redis_.Command({"DEL", WarAskOf(world, war.challenger, war.challenged)})
        .status()
        .IgnoreError();
  }
  if (!started.ok()) {
    return started;
  }
  war.left = kWarLength;
  return war;
}

absl::StatusOr<std::vector<War>> Guilds::Wars(const std::string& world) {
  const absl::StatusOr<Result> rows = database_.Run(
      "SELECT guild_wars.id, challenger, asking.name, challenged, asked.name, "
      "challenger_kills, challenged_kills, "
      "TIMESTAMPDIFF(SECOND, NOW(), ends_at) FROM guild_wars "
      "JOIN guilds AS asking ON asking.id = guild_wars.challenger "
      "JOIN guilds AS asked ON asked.id = guild_wars.challenged "
      "WHERE guild_wars.world = ? AND ends_at > NOW()",
      {world});
  if (!rows.ok()) {
    return rows.status();
  }
  std::vector<War> wars;
  wars.reserve(rows->rows.size());
  for (const std::vector<std::string>& row : rows->rows) {
    War& war = wars.emplace_back();
    int64_t left = 0;
    if (!absl::SimpleAtoi(row[0], &war.id) ||
        !absl::SimpleAtoi(row[1], &war.challenger) ||
        !absl::SimpleAtoi(row[3], &war.challenged) ||
        !absl::SimpleAtoi(row[5], &war.challenger_kills) ||
        !absl::SimpleAtoi(row[6], &war.challenged_kills) ||
        !absl::SimpleAtoi(row[7], &left)) {
      return absl::InternalError("a war that isn't numbers");
    }
    war.challenger_name = row[2];
    war.challenged_name = row[4];
    war.left = std::chrono::seconds(left);
  }
  return wars;
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
