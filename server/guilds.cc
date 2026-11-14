#include "server/guilds.h"

#include <cstddef>
#include <string>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"

namespace nanommo {

namespace {

// Each player has one invitation at a time, by their character's id, which
// holds the guild's.
std::string InvitationOf(uint32_t player) {
  return absl::StrCat("guild_invite:", player);
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

absl::Status Guilds::Decline(uint32_t player) {
  return redis_.Command({"DEL", InvitationOf(player)}).status();
}

}  // namespace nanommo
