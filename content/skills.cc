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
    uint32_t blow = 0;
    for (const skills::Blow& each : skill.effect().blows()) {
      if (each.at_ms() < blow) {
        return absl::InvalidArgumentError(
            absl::StrCat("the ", skill.name(), "'s blows go back in time"));
      }
      if (each.at_ms() > skill.effect().cast_ms()) {
        return absl::InvalidArgumentError(absl::StrCat(
            "the ", skill.name(), " lands a blow after it's over"));
      }
      blow = each.at_ms();
    }
    // A swing's as long as the weapon's, so only the others are checked.
    uint32_t last = 0;
    for (const skills::Key& key : skill.look().move().keys()) {
      if (key.at_ms() < last) {
        return absl::InvalidArgumentError(
            absl::StrCat("the ", skill.name(), "'s move goes back in time"));
      }
      last = key.at_ms();
    }
    if (!skill.effect().weapon() && last > skill.effect().cast_ms()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "the ", skill.name(), "'s move runs on after it's over"));
    }
  }
  std::set<uint32_t> starting(file.starting().begin(), file.starting().end());
  for (const uint32_t id : starting) {
    if (!by_id->contains(id)) {
      return absl::InvalidArgumentError(
          absl::StrCat("there's no skill ", id, " to start with"));
    }
  }
  for (const auto& [id, skill] : *by_id) {
    if (skill.next() != 0 && !by_id->contains(skill.next())) {
      return absl::InvalidArgumentError(absl::StrCat(
          "the ", skill.name(), " goes on into a skill that isn't there"));
    }
  }
  // Everyone has the swing, and the swings it goes on into, from the start.
  if (file.attack() != 0) {
    if (!by_id->contains(file.attack())) {
      return absl::InvalidArgumentError(
          absl::StrCat("there's no skill ", file.attack(), " to swing with"));
    }
    for (uint32_t id = file.attack(); id != 0 && !starting.contains(id);
         id = by_id->at(id).next()) {
      starting.insert(id);
    }
  }
  return SkillTable{.by_id = *std::move(by_id),
                    .starting = std::move(starting),
                    .attack = file.attack()};
}

}  // namespace nanommo
