#include "client/names.h"

#include <cstdint>
#include <string>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
#include "client/translations.h"
#include "protocol/messages.h"

namespace nanommo {

const char* NameOf(const items::Item& item) {
  return Tr(absl::StrCat("item ", item.id()), item.name().c_str());
}

const char* NameOf(const skills::Skill& skill) {
  return Tr(absl::StrCat("skill ", skill.id()), skill.name().c_str());
}

const char* NameOf(const monsters::Kind& kind) {
  return Tr(absl::StrCat("monster ", kind.id()), kind.name().c_str());
}

const char* NameOf(const npcs::Kind& kind) {
  return Tr(absl::StrCat("npc ", kind.id()), kind.name().c_str());
}

const char* NameOf(const quests::Quest& quest) {
  return Tr(absl::StrCat("quest ", quest.id()), quest.name().c_str());
}

const char* NameOf(const recipes::Recipe& recipe) {
  return Tr(absl::StrCat("recipe ", recipe.id()), recipe.name().c_str());
}

const char* NameOf(const factions::Faction& faction) {
  return Tr(absl::StrCat("faction ", faction.id()), faction.name().c_str());
}

const char* NameOf(const figures::Hair& hair) {
  return Tr("hair", hair.name().c_str());
}

const char* NameOf(const rig::Build& build) {
  return Tr("build", build.name().c_str());
}

const char* NameOf(uint32_t kind, const monsters::Skill& skill) {
  return Tr(absl::StrCat("monster ", kind, " skill ", skill.id()),
            skill.name().c_str());
}

const char* TextOf(const quests::Quest& quest) {
  return Tr(absl::StrCat("quest ", quest.id()), quest.text().c_str());
}

const char* NameOf(items::Slot slot) {
  switch (slot) {
    case items::SLOT_MAIN_HAND:
      return Tr("main hand");
    case items::SLOT_OFF_HAND:
      return Tr("off hand");
    case items::SLOT_HEAD:
      return Tr("head");
    case items::SLOT_BODY:
      return Tr("body");
    case items::SLOT_LEGS:
      return Tr("legs");
    case items::SLOT_FEET:
      return Tr("feet");
    default:
      return Tr("nowhere");
  }
}

std::string NameOf(const monsters::Kind& kind, uint32_t count) {
  const auto [one, many] = Counted(kind);
  return absl::Substitute(TrN(absl::StrCat("monster ", kind.id()), one.c_str(),
                              many.c_str(), count),
                          count);
}

std::string NameOf(const items::Item& item, uint32_t count) {
  const auto [one, many] = Counted(item);
  return absl::Substitute(
      TrN(absl::StrCat("item ", item.id()), one.c_str(), many.c_str(), count),
      count);
}

std::string LandName(std::string_view land) {
  return Tr("land", std::string(land).c_str());
}

}  // namespace nanommo
