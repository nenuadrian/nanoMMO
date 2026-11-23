#include "content/factions.h"

#include <cstdint>
#include <map>
#include <set>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "content/checks.h"
#include "content/text.h"

namespace nanommo {

absl::StatusOr<std::map<uint32_t, factions::Faction>> ParseFactions(
    std::string_view text) {
  factions::Factions file;
  const absl::Status parsed = ParseText(text, file);
  if (!parsed.ok()) {
    return parsed;
  }
  absl::StatusOr<std::map<uint32_t, factions::Faction>> by_id =
      ById(file.factions());
  if (!by_id.ok()) {
    return by_id.status();
  }
  // Zero is what protobuf gives a field we leave out: a faction nobody could
  // see by a name, or swear to. A herald swears players to one faction.
  std::set<uint32_t> heralds;
  for (const auto& [id, faction] : *by_id) {
    if (faction.color() == 0) {
      return absl::InvalidArgumentError(
          absl::StrCat(faction.name(), " has no colour"));
    }
    if (faction.herald() == 0) {
      return absl::InvalidArgumentError(
          absl::StrCat(faction.name(), " has no herald"));
    }
    if (!heralds.insert(faction.herald()).second) {
      return absl::InvalidArgumentError(
          absl::StrCat("two factions have the herald ", faction.herald()));
    }
  }
  return by_id;
}

const factions::Faction* HeraldOf(
    const std::map<uint32_t, factions::Faction>& factions, uint32_t kind) {
  for (const auto& [id, faction] : factions) {
    if (faction.herald() == kind) {
      return &faction;
    }
  }
  return nullptr;
}

}  // namespace nanommo
