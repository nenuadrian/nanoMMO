#include "server/players.h"

#include <map>
#include <string>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"

namespace nanommo {

namespace {

std::string Key(uint32_t account_id) {
  return absl::StrCat("player:", account_id);
}

}  // namespace

absl::StatusOr<Player> Players::Load(uint32_t account_id) {
  const absl::StatusOr<Result> result = database_.Run(
      "SELECT accounts.username, COALESCE(players.x, 0), "
      "COALESCE(players.z, 0), COALESCE(players.experience, 0) FROM accounts "
      "LEFT JOIN players ON players.account_id = accounts.id "
      "WHERE accounts.id = ?",
      {std::to_string(account_id)});
  if (!result.ok()) {
    return result.status();
  }
  if (result->rows.empty()) {
    return absl::NotFoundError(absl::StrCat("no account ", account_id));
  }
  const std::vector<std::string>& row = result->rows.front();
  Player player{.name = row[0]};
  std::vector<std::string> saved(row.begin() + 1, row.end());

  const absl::StatusOr<std::vector<std::vector<std::string>>> newer =
      redis_.Pipeline({{"HMGET", Key(account_id), "x", "z", "experience"}});
  if (!newer.ok()) {
    return newer.status();
  }
  if (!newer->front()[0].empty()) {
    saved = newer->front();
  }
  if (!absl::SimpleAtof(saved[0], &player.x) ||
      !absl::SimpleAtof(saved[1], &player.z) ||
      !absl::SimpleAtoi(saved[2], &player.experience)) {
    return absl::InternalError("a saved player that isn't numbers");
  }
  return player;
}

absl::Status Players::Save(const std::map<uint32_t, Progress>& changed) {
  std::vector<std::vector<std::string>> commands;
  for (const auto& [id, progress] : changed) {
    commands.push_back({"HSET", Key(id), "x", absl::StrCat(progress.x), "z",
                        absl::StrCat(progress.z), "experience",
                        absl::StrCat(progress.experience)});
  }
  return redis_.Pipeline(commands).status();
}

}  // namespace nanommo
