#include "server/characters.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "server/accounts.h"
#include "server/levels.h"

namespace nanommo {

absl::StatusOr<std::vector<protocol::CharacterListing>> Characters::List(
    uint32_t account) {
  const absl::StatusOr<Result> rows = database_.Run(
      "SELECT id, name, world, experience, land, COALESCE(weapon, 0), "
      "items_revision IS NULL "
      "FROM players WHERE account_id = ? ORDER BY id",
      {absl::StrCat(account)});
  if (!rows.ok()) {
    return rows.status();
  }
  std::vector<protocol::CharacterListing> characters;
  std::map<uint32_t, protocol::CharacterListing*> by_id;
  characters.reserve(rows->rows.size());
  for (const std::vector<std::string>& row : rows->rows) {
    protocol::CharacterListing& character = characters.emplace_back();
    uint32_t id = 0;
    uint32_t experience = 0;
    uint32_t weapon = 0;
    if (!absl::SimpleAtoi(row[0], &id) ||
        !absl::SimpleAtoi(row[3], &experience) ||
        !absl::SimpleAtoi(row[5], &weapon)) {
      return absl::InternalError("a character that isn't numbers");
    }
    character.set_id(id);
    character.set_name(row[1]);
    character.set_world(row[2]);
    character.set_level(static_cast<uint32_t>(LevelFor(experience).number));
    character.set_land(row[4]);
    character.set_weapon(weapon);
    character.set_fresh(row[6] == "1");
  }
  for (protocol::CharacterListing& character : characters) {
    by_id[character.id()] = &character;
  }
  if (characters.empty()) {
    return characters;
  }
  // What they wear: whatever's in a slot, not in the bag. The main hand's is
  // the weapon, which the column above had for a character from before bags.
  const absl::StatusOr<Result> worn = database_.Run(
      "SELECT player_items.player, player_items.place, player_items.item "
      "FROM player_items JOIN players ON players.id = player_items.player "
      "WHERE players.account_id = ? AND player_items.place NOT LIKE 'bag %' "
      "ORDER BY player_items.player, player_items.place",
      {absl::StrCat(account)});
  if (!worn.ok()) {
    return worn.status();
  }
  for (const std::vector<std::string>& row : worn->rows) {
    uint32_t id = 0;
    uint32_t item = 0;
    if (!absl::SimpleAtoi(row[0], &id) || !absl::SimpleAtoi(row[2], &item)) {
      return absl::InternalError("a worn item that isn't numbers");
    }
    const auto found = by_id.find(id);
    if (found == by_id.end()) {
      continue;
    }
    if (row[1] == "main hand") {
      found->second->set_weapon(item);
    } else {
      found->second->add_wearing(item);
    }
  }
  return characters;
}

absl::StatusOr<uint32_t> Characters::Create(uint32_t account,
                                            const std::string& world,
                                            const std::string& name) {
  if (const absl::Status checked = CheckUsername(name); !checked.ok()) {
    return checked;
  }
  // Counting and inserting in one statement, so that two at once can't both
  // fit in the last place. The unique key on the world and the name says
  // whether it's taken.
  const absl::StatusOr<Result> inserted = database_.Run(
      "INSERT INTO players (account_id, world, name, x, z) "
      "SELECT ?, ?, ?, 0, 0 FROM DUAL WHERE "
      "(SELECT COUNT(*) FROM players WHERE account_id = ? AND world = ?) < ?",
      {absl::StrCat(account), world, name, absl::StrCat(account), world,
       absl::StrCat(kPerWorld)});
  if (absl::IsAlreadyExists(inserted.status())) {
    return absl::AlreadyExistsError(
        absl::StrCat("Someone in ", world, " is called that already."));
  }
  if (!inserted.ok()) {
    return inserted.status();
  }
  if (inserted->insert_id == 0) {
    return absl::ResourceExhaustedError(absl::StrCat(
        "You have ", kPerWorld, " characters in ", world, " already."));
  }
  return static_cast<uint32_t>(inserted->insert_id);
}

absl::Status Characters::Delete(uint32_t account, uint32_t id) {
  const absl::StatusOr<Result> deleted =
      database_.Run("DELETE FROM players WHERE id = ? AND account_id = ?",
                    {absl::StrCat(id), absl::StrCat(account)});
  if (!deleted.ok()) {
    return deleted.status();
  }
  if (deleted->affected_rows == 0) {
    return absl::NotFoundError("That character isn't yours.");
  }
  return absl::OkStatus();
}

absl::StatusOr<std::string> Characters::WorldOf(uint32_t account,
                                                uint32_t id) {
  const absl::StatusOr<Result> rows = database_.Run(
      "SELECT world FROM players WHERE id = ? AND account_id = ?",
      {absl::StrCat(id), absl::StrCat(account)});
  if (!rows.ok()) {
    return rows.status();
  }
  if (rows->rows.empty()) {
    return absl::NotFoundError("That character isn't yours.");
  }
  return rows->rows.front()[0];
}

}  // namespace nanommo
