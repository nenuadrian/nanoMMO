#include "server/players.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"

namespace nanommo {

namespace {

constexpr char kUnsaved[] = "players:unsaved";

std::string Key(const absl::AlphaNum& account_id) {
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
  if (changed.empty()) {
    return absl::OkStatus();
  }
  std::vector<std::vector<std::string>> commands;
  std::vector<std::string> unsaved = {"SADD", kUnsaved};
  for (const auto& [id, progress] : changed) {
    commands.push_back({"HSET", Key(id), "x", absl::StrCat(progress.x), "z",
                        absl::StrCat(progress.z), "experience",
                        absl::StrCat(progress.experience)});
    unsaved.push_back(absl::StrCat(id));
  }
  commands.push_back(unsaved);
  return redis_.Pipeline(commands).status();
}

absl::StatusOr<int> Players::Flush() {
  // As many as are unsaved now: whoever changes meanwhile waits for next time.
  const absl::StatusOr<std::string> count = redis_.Command({"SCARD", kUnsaved});
  int left = 0;
  if (!count.ok() || !absl::SimpleAtoi(*count, &left)) {
    return absl::UnavailableError("couldn't count the unsaved players");
  }
  int saved = 0;
  while (left > 0) {
    // SPOP takes them out of the set, so one who changes after this is marked
    // unsaved again, and goes in a later batch.
    const absl::StatusOr<std::vector<std::vector<std::string>>> popped =
        redis_.Pipeline(
            {{"SPOP", kUnsaved, absl::StrCat(std::min(left, kBatch))}});
    if (!popped.ok()) {
      return popped.status();
    }
    const std::vector<std::string>& ids = popped->front();
    if (ids.empty()) {
      break;
    }
    left -= static_cast<int>(ids.size());

    std::vector<std::vector<std::string>> reads;
    for (const std::string& id : ids) {
      reads.push_back({"HMGET", Key(id), "x", "z", "experience"});
    }
    const absl::StatusOr<std::vector<std::vector<std::string>>> hashes =
        redis_.Pipeline(reads);
    if (!hashes.ok()) {
      return hashes.status();
    }

    // IGNORE skips a player whose account is gone, with a warning, so that one
    // row MySQL refuses doesn't hold back the batch on each flush.
    std::string sql =
        "INSERT IGNORE INTO players (account_id, x, z, experience) VALUES ";
    std::vector<std::string> values;
    int rows = 0;
    for (std::size_t i = 0; i < ids.size(); ++i) {
      // A player whose hash is gone has nothing to save.
      if ((*hashes)[i][0].empty()) {
        continue;
      }
      absl::StrAppend(&sql, rows == 0 ? "" : ", ", "(?, ?, ?, ?)");
      ++rows;
      values.push_back(ids[i]);
      values.insert(values.end(), (*hashes)[i].begin(), (*hashes)[i].end());
    }
    if (rows == 0) {
      continue;
    }
    absl::StrAppend(&sql,
                    " AS saved ON DUPLICATE KEY UPDATE x = saved.x, "
                    "z = saved.z, experience = saved.experience");
    const absl::StatusOr<Result> result = database_.Run(sql, values);
    if (!result.ok()) {
      // Back into the set, so that the next flush tries them again.
      std::vector<std::string> again = {"SADD", kUnsaved};
      again.insert(again.end(), ids.begin(), ids.end());
      redis_.Pipeline({again}).IgnoreError();
      return result.status();
    }
    saved += rows;
  }
  return saved;
}

}  // namespace nanommo
