#include "content/monsters.h"

#include <cmath>
#include <cstdint>
#include <map>
#include <string_view>
#include <utility>

#include "absl/strings/str_cat.h"
#include "content/checks.h"
#include "content/text.h"

namespace nanommo {

absl::StatusOr<MonsterTable> ParseMonsters(std::string_view text) {
  monsters::Monsters file;
  const absl::Status parsed = ParseText(text, file);
  if (!parsed.ok()) {
    return parsed;
  }
  absl::StatusOr<std::map<uint32_t, monsters::Kind>> kinds = ById(file.kinds());
  if (!kinds.ok()) {
    return kinds.status();
  }
  MonsterTable table{.kinds = *std::move(kinds),
                     .spawns = {file.spawns().begin(), file.spawns().end()},
                     .camps = {file.camps().begin(), file.camps().end()}};
  for (const monsters::Spawn& spawn : table.spawns) {
    const absl::Status checked = CheckSpawn(table.kinds, spawn);
    if (!checked.ok()) {
      return checked;
    }
    // Players come back to life in the middle of their land, so a monster
    // can't chase them there.
    const monsters::Kind& kind = table.kinds.at(spawn.kind());
    if (std::hypot(spawn.x(), spawn.z()) <= kind.fight().leash()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "the ", kind.name(), " at ", spawn.x(), ", ", spawn.z(),
          " could chase players in the middle of the ", spawn.land()));
    }
  }
  for (const monsters::Camps& camps : table.camps) {
    const absl::Status checked = CheckSpawn(table.kinds, camps);
    if (!checked.ok()) {
      return checked;
    }
  }
  return table;
}

}  // namespace nanommo
