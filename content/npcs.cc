#include "content/npcs.h"

#include <cstdint>
#include <map>
#include <string_view>
#include <utility>

#include "content/checks.h"
#include "content/text.h"

namespace nanommo {

absl::StatusOr<NpcTable> ParseNpcs(std::string_view text) {
  npcs::Npcs file;
  const absl::Status parsed = ParseText(text, file);
  if (!parsed.ok()) {
    return parsed;
  }
  absl::StatusOr<std::map<uint32_t, npcs::Kind>> kinds = ById(file.kinds());
  if (!kinds.ok()) {
    return kinds.status();
  }
  NpcTable table{.kinds = *std::move(kinds),
                 .spawns = {file.spawns().begin(), file.spawns().end()}};
  for (const npcs::Spawn& spawn : table.spawns) {
    const absl::Status checked = CheckSpawn(table.kinds, spawn);
    if (!checked.ok()) {
      return checked;
    }
  }
  return table;
}

}  // namespace nanommo
