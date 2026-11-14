#include "server/guilds.h"

#include <cstddef>
#include <string>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"

namespace nanommo {

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

}  // namespace nanommo
