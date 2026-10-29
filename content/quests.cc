#include "content/quests.h"

#include <cstdint>
#include <map>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "content/checks.h"
#include "content/text.h"

namespace nanommo {

absl::StatusOr<std::map<uint32_t, quests::Quest>> ParseQuests(
    std::string_view text) {
  quests::Quests file;
  const absl::Status parsed = ParseText(text, file);
  if (!parsed.ok()) {
    return parsed;
  }
  absl::StatusOr<std::map<uint32_t, quests::Quest>> by_id = ById(file.quests());
  if (!by_id.ok()) {
    return by_id.status();
  }
  // Zero is what protobuf gives a field we leave out, so each of these
  // would be a quest nobody could take, or finish.
  for (const auto& [id, quest] : *by_id) {
    if (quest.giver() == 0) {
      return absl::InvalidArgumentError(
          absl::StrCat("the ", quest.name(), " has no giver"));
    }
    if (quest.kill().count() == 0) {
      return absl::InvalidArgumentError(
          absl::StrCat("the ", quest.name(), " asks for no kills"));
    }
    for (const quests::Prize& prize : quest.prizes()) {
      if (prize.count() == 0) {
        return absl::InvalidArgumentError(absl::StrCat(
            "the ", quest.name(), " gives none of item ", prize.item()));
      }
    }
  }
  return by_id;
}

}  // namespace nanommo
