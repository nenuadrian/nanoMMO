#include "content/skills.h"

#include <cstdint>
#include <map>
#include <set>
#include <string_view>
#include <utility>

#include "absl/strings/str_cat.h"
#include "content/checks.h"
#include "content/text.h"

namespace nanommo {

absl::StatusOr<SkillTable> ParseSkills(std::string_view text) {
  skills::Skills file;
  const absl::Status parsed = ParseText(text, file);
  if (!parsed.ok()) {
    return parsed;
  }
  absl::StatusOr<std::map<uint32_t, skills::Skill>> by_id = ById(file.skills());
  if (!by_id.ok()) {
    return by_id.status();
  }
  for (const auto& [id, skill] : *by_id) {
    // Zero is what protobuf gives a level we leave out, so a skill whose
    // level we forgot would be everyone's, with no error.
    if (skill.level() == 0) {
      return absl::InvalidArgumentError(
          absl::StrCat("the ", skill.name(), " has no level"));
    }
    if (skill.effect().hit_ms() > skill.effect().cast_ms()) {
      return absl::InvalidArgumentError(
          absl::StrCat("the ", skill.name(), " lands after it's over"));
    }
  }
  std::set<uint32_t> starting(file.starting().begin(), file.starting().end());
  for (const uint32_t id : starting) {
    if (!by_id->contains(id)) {
      return absl::InvalidArgumentError(
          absl::StrCat("there's no skill ", id, " to start with"));
    }
  }
  return SkillTable{.by_id = *std::move(by_id),
                    .starting = std::move(starting)};
}

}  // namespace nanommo
