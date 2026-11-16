#include "content/monsters.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>

#include "absl/strings/str_cat.h"
#include "content/checks.h"
#include "content/text.h"

namespace nanommo {

namespace {

// Whether players can see the skill coming and step out of it: it has a
// shape, the player it's fighting is in it as it starts, it shows for long
// enough, and it can't start again before it's landed.
absl::Status CheckSkill(const monsters::Kind& kind,
                        const monsters::Skill& skill) {
  const std::string name =
      absl::StrCat("the ", kind.name(), "'s ", skill.name());
  if (skill.size() <= 0.0f) {
    return absl::InvalidArgumentError(absl::StrCat(name, " has no size"));
  }
  if (skill.shape() == monsters::CONE && skill.arc() <= 0.0f) {
    return absl::InvalidArgumentError(
        absl::StrCat(name, " is a cone without an arc"));
  }
  if (skill.range() <= 0.0f) {
    return absl::InvalidArgumentError(absl::StrCat(name, " has no range"));
  }
  if (skill.range() > skill.size()) {
    return absl::InvalidArgumentError(
        absl::StrCat(name, " starts from ", skill.range(),
                     " metres, outside its ", skill.size()));
  }
  if (std::chrono::milliseconds(skill.telegraph_ms()) < kShortestTelegraph) {
    return absl::InvalidArgumentError(
        absl::StrCat(name, " shows for ", skill.telegraph_ms(),
                     " ms, too short to step out of: it needs ",
                     kShortestTelegraph.count(), " at least"));
  }
  if (skill.cooldown_ms() < skill.telegraph_ms()) {
    return absl::InvalidArgumentError(
        absl::StrCat(name, " cools down in ", skill.cooldown_ms(),
                     " ms, before it lands, at ", skill.telegraph_ms()));
  }
  return absl::OkStatus();
}

// Whether a boss's phases go down its health, from the first, at full health,
// each lower than the last, while it's alive, use skills it has, and call up
// some of kinds that there are.
absl::Status CheckPhases(const monsters::Kind& kind,
                         const std::map<uint32_t, monsters::Kind>& kinds) {
  for (int i = 0; i < kind.phases_size(); ++i) {
    const monsters::Phase& phase = kind.phases(i);
    const std::string name =
        absl::StrCat("the ", kind.name(), "'s phase ", i + 1);
    if (i == 0 && phase.health() != 100) {
      return absl::InvalidArgumentError(absl::StrCat(
          name, " starts at ", phase.health(), "% of its health, not at 100%"));
    }
    if (i > 0 && phase.health() == 0) {
      return absl::InvalidArgumentError(
          absl::StrCat(name, " starts at 0%, when it's dead"));
    }
    if (i > 0 && phase.health() >= kind.phases(i - 1).health()) {
      return absl::InvalidArgumentError(absl::StrCat(
          name, " starts at ", phase.health(), "%, which isn't below phase ", i,
          "'s ", kind.phases(i - 1).health(), "%"));
    }
    for (const uint32_t skill : phase.skills()) {
      if (std::ranges::find(kind.skills(), skill, &monsters::Skill::id) ==
          kind.skills().end()) {
        return absl::InvalidArgumentError(
            absl::StrCat(name, " uses a skill ", skill, " it doesn't have"));
      }
    }
    for (const monsters::Add& add : phase.adds()) {
      const auto called = kinds.find(add.kind());
      if (called == kinds.end()) {
        return absl::InvalidArgumentError(absl::StrCat(
            name, " calls up a kind ", add.kind(), " that isn't in the file"));
      }
      if (add.count() == 0) {
        return absl::InvalidArgumentError(
            absl::StrCat(name, " calls up no ", called->second.name(), "s"));
      }
    }
  }
  return absl::OkStatus();
}

}  // namespace

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
  for (const auto& [id, kind] : *kinds) {
    for (const monsters::Drop& drop : kind.loot()) {
      if (drop.chance() <= 0.0f || drop.chance() > 1.0f) {
        return absl::InvalidArgumentError(
            absl::StrCat("the ", kind.name(), " drops item ", drop.item(),
                         " with a chance of ", drop.chance(),
                         ", which has to be more than 0 and at most 1"));
      }
      if (drop.max() != 0 && drop.max() < drop.min()) {
        return absl::InvalidArgumentError(
            absl::StrCat("the ", kind.name(), " drops from ", drop.min(),
                         " to ", drop.max(), " of item ", drop.item()));
      }
    }
    if (kind.gold().max() < kind.gold().min()) {
      return absl::InvalidArgumentError(
          absl::StrCat("the ", kind.name(), " drops from ", kind.gold().min(),
                       " to ", kind.gold().max(), " gold"));
    }
    // The clients hear which skill a monster's using by its id, as they do
    // its kind.
    if (const absl::Status ids = ById(kind.skills()).status(); !ids.ok()) {
      return ids;
    }
    for (const monsters::Skill& skill : kind.skills()) {
      if (const absl::Status checked = CheckSkill(kind, skill); !checked.ok()) {
        return checked;
      }
    }
    if (const absl::Status checked = CheckPhases(kind, *kinds); !checked.ok()) {
      return checked;
    }
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

bool InShape(const monsters::Skill& skill, Place from, float yaw, Place place) {
  const float dx = place.x - from.x;
  const float dz = place.z - from.z;
  if (std::hypot(dx, dz) > skill.size()) {
    return false;
  }
  if (skill.shape() != monsters::CONE) {
    return true;
  }
  // As a player's slice is: half the arc to either side of the way it faced.
  const float off = std::remainder(std::atan2(dx, dz) - yaw,
                                   2.0f * std::numbers::pi_v<float>);
  return std::abs(off) <=
         skill.arc() / 2.0f * std::numbers::pi_v<float> / 180.0f;
}

}  // namespace nanommo
