#include "protocol/messages.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"

namespace nanommo {

namespace {

// Where a Refusal keeps its sentence in a status, by the message's type.
constexpr std::string_view kPayload =
    "type.googleapis.com/nanommo.protocol.Message";

Part Made(protocol::Arg arg, std::string english) {
  return {.arg = std::move(arg), .english = std::move(english)};
}

// The English of so many, from a plural's two forms.
std::string Plural(const std::pair<std::string, std::string>& forms,
                   uint32_t count) {
  const std::string counted = absl::StrCat(count);
  return Fill(count == 1 ? forms.first : forms.second, {&counted, 1});
}

template <typename Table>
const auto* Find(const Table& table, uint32_t id) {
  const auto found = table.find(id);
  return found != table.end() ? &found->second : nullptr;
}

// An arg's English, from the content's names by their ids, as it was made.
std::string EnglishOf(const protocol::Arg& arg, const Content& content) {
  // What the content hasn't got.
  const std::string something = "something";
  switch (arg.value_case()) {
    case protocol::Arg::kNumber:
      return absl::StrCat(arg.number());
    case protocol::Arg::kName:
      return arg.name();
    case protocol::Arg::kPhrase:
      return English(arg.phrase(), content);
    case protocol::Arg::kItem: {
      const auto* item = Find(content.items.by_id, arg.item());
      return item != nullptr ? Of(*item, arg.count()).english : something;
    }
    case protocol::Arg::kMonster: {
      const auto* kind = Find(content.monsters.kinds, arg.monster());
      return kind != nullptr ? Of(*kind, arg.count()).english : something;
    }
    case protocol::Arg::kNpc: {
      const auto* kind = Find(content.npcs.kinds, arg.npc());
      return kind != nullptr ? Of(*kind).english : something;
    }
    case protocol::Arg::kQuest: {
      const auto* quest = Find(content.quests, arg.quest());
      return quest != nullptr ? Of(*quest).english : something;
    }
    case protocol::Arg::kSkill: {
      const auto* skill = Find(content.skills.by_id, arg.skill());
      return skill != nullptr ? Of(*skill).english : something;
    }
    case protocol::Arg::kSlot:
      return Of(arg.slot()).english;
    case protocol::Arg::VALUE_NOT_SET:
      break;
  }
  return "";
}

}  // namespace

std::string Fill(std::string_view text, std::span<const std::string> args) {
  std::string filled;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char next = i + 1 < text.size() ? text[i + 1] : '\0';
    if (text[i] == '$' && next == '$') {
      filled += '$';
      ++i;
    } else if (text[i] == '$' && absl::ascii_isdigit(next)) {
      if (const auto at = static_cast<std::size_t>(next - '0');
          at < args.size()) {
        filled += args[at];
      }
      ++i;
    } else {
      filled += text[i];
    }
  }
  return filled;
}

Part Number(uint64_t number) {
  protocol::Arg arg;
  arg.set_number(number);
  return Made(std::move(arg), absl::StrCat(number));
}

Part Name(std::string_view name) {
  protocol::Arg arg;
  arg.set_name(name);
  return Made(std::move(arg), std::string(name));
}

std::pair<std::string, std::string> Counted(const items::Item& item) {
  const std::string_view name = item.name();
  // A name that's plural already, such as trousers, shoes or boots, comes in
  // pairs.
  if (name.ends_with('s')) {
    return {absl::StrCat("a pair of ", name),
            absl::StrCat("$0 pairs of ", name)};
  }
  const bool vowel =
      !name.empty() &&
      std::string_view("AEIOU").find(name.front()) != std::string_view::npos;
  return {absl::StrCat(vowel ? "an " : "a ", name),
          absl::StrCat("$0 ", name, "s")};
}

std::pair<std::string, std::string> Counted(const monsters::Kind& kind) {
  return {absl::StrCat("$0 ", kind.name()),
          absl::StrCat("$0 ", kind.name(), "s")};
}

Part Of(const items::Item& item, uint32_t count) {
  protocol::Arg arg;
  arg.set_item(item.id());
  arg.set_count(count);
  return Made(std::move(arg),
              count == 0 ? item.name() : Plural(Counted(item), count));
}

Part Of(const ItemTable& items, uint32_t item, uint32_t count) {
  const items::Item* found = Find(items.by_id, item);
  return found != nullptr ? Of(*found, count) : Of(Msg("something"));
}

Part Of(const monsters::Kind& kind, uint32_t count) {
  protocol::Arg arg;
  arg.set_monster(kind.id());
  arg.set_count(count);
  return Made(std::move(arg),
              count == 0 ? kind.name() : Plural(Counted(kind), count));
}

Part Of(const npcs::Kind& kind) {
  protocol::Arg arg;
  arg.set_npc(kind.id());
  return Made(std::move(arg), kind.name());
}

Part Of(const quests::Quest& quest) {
  protocol::Arg arg;
  arg.set_quest(quest.id());
  return Made(std::move(arg), quest.name());
}

Part Of(const skills::Skill& skill) {
  protocol::Arg arg;
  arg.set_skill(skill.id());
  return Made(std::move(arg), skill.name());
}

Part Of(items::Slot slot) {
  protocol::Arg arg;
  arg.set_slot(slot);
  return Made(std::move(arg), std::string(SlotName(slot)));
}

Part Of(Sentence phrase) {
  protocol::Arg arg;
  *arg.mutable_phrase() = std::move(phrase.message);
  return Made(std::move(arg), std::move(phrase.english));
}

namespace messages_internal {

Sentence Make(std::string_view one, std::string_view many, uint64_t count,
              std::vector<Part> parts) {
  Sentence made;
  made.message.set_text(one);
  if (!many.empty()) {
    made.message.set_plural(many);
    made.message.set_count(count);
  }
  std::vector<std::string> english;
  english.reserve(parts.size());
  for (Part& part : parts) {
    *made.message.add_args() = std::move(part.arg);
    english.push_back(std::move(part.english));
  }
  made.english = Fill(!many.empty() && count != 1 ? many : one, english);
  return made;
}

}  // namespace messages_internal

absl::Status Refusal(absl::StatusCode code, const Sentence& why) {
  absl::Status refused(code, why.english);
  refused.SetPayload(kPayload, absl::Cord(why.message.SerializeAsString()));
  return refused;
}

Sentence SentenceOf(const absl::Status& status) {
  Sentence said;
  said.english = std::string(status.message());
  const std::optional<absl::Cord> payload = status.GetPayload(kPayload);
  if (!payload || !said.message.ParseFromString(std::string(*payload))) {
    said.message.Clear();
    said.message.set_text(said.english);
  }
  return said;
}

std::string English(const protocol::Message& message, const Content& content) {
  std::vector<std::string> args;
  args.reserve(message.args_size());
  for (const protocol::Arg& arg : message.args()) {
    args.push_back(EnglishOf(arg, content));
  }
  const bool many = !message.plural().empty() && message.count() != 1;
  return Fill(many ? message.plural() : message.text(), args);
}

}  // namespace nanommo
