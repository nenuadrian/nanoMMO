#include "server/players.h"

#include <string>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"

namespace nanommo {

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
  if (!absl::SimpleAtof(row[1], &player.x) ||
      !absl::SimpleAtof(row[2], &player.z) ||
      !absl::SimpleAtoi(row[3], &player.experience)) {
    return absl::InternalError("a saved player that isn't numbers");
  }
  return player;
}

absl::Status Players::Save(uint32_t account_id, const Player& player) {
  const absl::StatusOr<Result> result = database_.Run(
      "INSERT INTO players (account_id, x, z, experience) "
      "VALUES (?, ?, ?, ?) AS saved ON DUPLICATE KEY UPDATE "
      "x = saved.x, z = saved.z, experience = saved.experience",
      {std::to_string(account_id), std::to_string(player.x),
       std::to_string(player.z), std::to_string(player.experience)});
  return result.status();
}

}  // namespace nanommo
