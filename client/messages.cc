#include "client/messages.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "client/names.h"
#include "client/translations.h"
#include "protocol/messages.h"

namespace nanommo {

namespace {

template <typename Table>
const auto* Find(const Table& table, uint32_t id) {
  const auto found = table.find(id);
  return found != table.end() ? &found->second : nullptr;
}

std::string Translated(const protocol::Message& message,
                       const Content* content);

// The content's name, by the arg's id, in the language in use, or nothing
// for what this client's content hasn't got, which ClientHash keeps from
// happening.
std::optional<std::string> NameIn(const protocol::Arg& arg,
                                  const Content& content) {
  switch (arg.value_case()) {
    case protocol::Arg::kItem:
      if (const auto* item = Find(content.items.by_id, arg.item())) {
        return arg.count() == 0 ? NameOf(*item) : NameOf(*item, arg.count());
      }
      break;
    case protocol::Arg::kMonster:
      if (const auto* kind = Find(content.monsters.kinds, arg.monster())) {
        return arg.count() == 0 ? NameOf(*kind) : NameOf(*kind, arg.count());
      }
      break;
    case protocol::Arg::kNpc:
      if (const auto* kind = Find(content.npcs.kinds, arg.npc())) {
        return NameOf(*kind);
      }
      break;
    case protocol::Arg::kQuest:
      if (const auto* quest = Find(content.quests, arg.quest())) {
        return NameOf(*quest);
      }
      break;
    case protocol::Arg::kSkill:
      if (const auto* skill = Find(content.skills.by_id, arg.skill())) {
        return NameOf(*skill);
      }
      break;
    case protocol::Arg::kFaction:
      if (const auto* faction = Find(content.factions, arg.faction())) {
        return NameOf(*faction);
      }
      break;
    default:
      break;
  }
  return std::nullopt;
}

// An arg as the language in use says it.
std::string Shown(const protocol::Arg& arg, const Content* content) {
  switch (arg.value_case()) {
    case protocol::Arg::kNumber:
      return absl::StrCat(arg.number());
    case protocol::Arg::kName:
      return arg.name();
    case protocol::Arg::kPhrase:
      return Translated(arg.phrase(), content);
    case protocol::Arg::kSlot:
      return NameOf(arg.slot());
    case protocol::Arg::VALUE_NOT_SET:
      return "";
    default:
      break;
  }
  std::optional<std::string> name;
  if (content != nullptr) {
    name = NameIn(arg, *content);
  }
  return name ? *std::move(name) : Tr("something");
}

std::string Translated(const protocol::Message& message,
                       const Content* content) {
  std::vector<std::string> args;
  args.reserve(message.args_size());
  for (const protocol::Arg& arg : message.args()) {
    args.push_back(Shown(arg, content));
  }
  const char* text = message.plural().empty()
                         ? Tr(message.text().c_str())
                         : TrN(message.text().c_str(), message.plural().c_str(),
                               message.count());
  return Fill(text, args);
}

}  // namespace

std::string Translated(const protocol::Message& message,
                       const Content& content) {
  return Translated(message, &content);
}

std::string Translated(const protocol::Message& message) {
  return Translated(message, nullptr);
}

}  // namespace nanommo
