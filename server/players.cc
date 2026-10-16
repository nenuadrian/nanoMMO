#include "server/players.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "terrain/lands.h"

namespace nanommo {

namespace {

constexpr char kUnsaved[] = "players:unsaved";

std::string Key(const std::string& world, uint32_t account_id) {
  return absl::StrCat("player:", world, ":", account_id);
}

}  // namespace

absl::StatusOr<Player> Players::Load(const std::string& world,
                                     uint32_t account_id) {
  const absl::StatusOr<Result> result = database_.Run(
      "SELECT accounts.username, COALESCE(players.x, 0), "
      "COALESCE(players.z, 0), COALESCE(players.experience, 0), "
      "players.health, COALESCE(players.land, 'Meadows'), players.weapon "
      "FROM accounts "
      "LEFT JOIN players ON players.account_id = accounts.id "
      "AND players.world = ? "
      "WHERE accounts.id = ?",
      {world, std::to_string(account_id)});
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
      redis_.Pipeline({{"HMGET", Key(world, account_id), "x", "z", "experience",
                        "health", "land", "weapon"}});
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
  if (!saved[3].empty() && !absl::SimpleAtoi(saved[3], &player.health)) {
    return absl::InternalError("a saved health that isn't a number");
  }
  player.land = LandNamed(saved[4]);
  // None saved, which the world takes as the starting weapon.
  if (!saved[5].empty() && !absl::SimpleAtoi(saved[5], &player.weapon)) {
    return absl::InternalError("a saved weapon that isn't a number");
  }
  return player;
}

absl::Status Players::Save(const std::string& world,
                           const std::map<uint32_t, Progress>& changed) {
  if (changed.empty()) {
    return absl::OkStatus();
  }
  std::vector<std::vector<std::string>> commands;
  std::vector<std::string> unsaved = {"SADD", kUnsaved};
  for (const auto& [id, progress] : changed) {
    commands.push_back(
        {"HSET", Key(world, id), "world", world, "account", absl::StrCat(id),
         "x", absl::StrCat(progress.x), "z", absl::StrCat(progress.z),
         "experience", absl::StrCat(progress.experience), "health",
         absl::StrCat(progress.health), "land", kLands[progress.land].name,
         "weapon", absl::StrCat(progress.weapon)});
    unsaved.push_back(Key(world, id));
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
    const std::vector<std::string>& keys = popped->front();
    if (keys.empty()) {
      break;
    }
    left -= static_cast<int>(keys.size());

    std::vector<std::vector<std::string>> reads;
    for (const std::string& key : keys) {
      // Before worlds, the set held account IDs, for hashes with no world.
      if (key.starts_with("player:")) {
        reads.push_back({"HMGET", key, "world", "account", "x", "z",
                         "experience", "health", "land", "weapon"});
      } else {
        reads.push_back({"HMGET", absl::StrCat("player:", key), "x", "z",
                         "experience", "health", "land", "weapon"});
      }
    }
    const absl::StatusOr<std::vector<std::vector<std::string>>> hashes =
        redis_.Pipeline(reads);
    if (!hashes.ok()) {
      return hashes.status();
    }

    // IGNORE skips a player whose account is gone, with a warning, so that one
    // row MySQL refuses doesn't hold back the batch on each flush.
    std::string sql =
        "INSERT IGNORE INTO players (world, account_id, x, z, experience, "
        "health, land, weapon) VALUES ";
    std::vector<std::string> values;
    int rows = 0;
    for (std::size_t i = 0; i < keys.size(); ++i) {
      // A player whose hash is gone has nothing to save.
      if ((*hashes)[i][0].empty()) {
        continue;
      }
      // A hash saved before we kept health or weapons has none, which goes in
      // as NULL, and one saved before lands is in the Meadows.
      absl::StrAppend(&sql, rows == 0 ? "" : ", ",
                      "(?, ?, ?, ?, ?, NULLIF(?, ''), "
                      "COALESCE(NULLIF(?, ''), 'Meadows'), NULLIF(?, ''))");
      ++rows;
      if (!keys[i].starts_with("player:")) {
        values.push_back("Ember");
        values.push_back(keys[i]);
      }
      values.insert(values.end(), (*hashes)[i].begin(), (*hashes)[i].end());
    }
    if (rows == 0) {
      continue;
    }
    absl::StrAppend(&sql,
                    " AS saved ON DUPLICATE KEY UPDATE x = saved.x, "
                    "z = saved.z, experience = saved.experience, "
                    "health = saved.health, land = saved.land, "
                    "weapon = saved.weapon");
    const absl::StatusOr<Result> result = database_.Run(sql, values);
    if (!result.ok()) {
      // Back into the set, so that the next flush tries them again.
      std::vector<std::string> again = {"SADD", kUnsaved};
      again.insert(again.end(), keys.begin(), keys.end());
      redis_.Pipeline({again}).IgnoreError();
      return result.status();
    }
    saved += rows;
  }
  return saved;
}

}  // namespace nanommo
