#include "server/events.h"

#include <string>
#include <string_view>
#include <vector>

#include "nlohmann/json.hpp"
#include "terrain/lands.h"

namespace nanommo {

namespace {

std::string_view Name(Event::Kind kind) {
  switch (kind) {
    case Event::Kind::kKill:
      return "kill";
    case Event::Kind::kDeath:
      return "death";
    case Event::Kind::kLoot:
      return "loot";
    case Event::Kind::kPrize:
      return "prize";
    case Event::Kind::kLevel:
      return "level";
    case Event::Kind::kTake:
      return "take";
    case Event::Kind::kTurnIn:
      return "turn_in";
    case Event::Kind::kUse:
      return "use";
    case Event::Kind::kHit:
      return "hit";
    case Event::Kind::kHurt:
      return "hurt";
    case Event::Kind::kHeal:
      return "heal";
  }
  return "";
}

// The name of what the event's about: the monster killed, or that killed the
// player, the item, or the quest.
std::string What(const Event& event, const Content& content) {
  switch (event.kind) {
    case Event::Kind::kKill:
    case Event::Kind::kDeath:
    case Event::Kind::kHit:
    case Event::Kind::kHurt: {
      const auto kind = content.monsters.kinds.find(event.monster);
      return kind != content.monsters.kinds.end() ? kind->second.name() : "";
    }
    case Event::Kind::kLoot:
    case Event::Kind::kPrize:
    case Event::Kind::kUse: {
      const auto item = content.items.by_id.find(event.item);
      return item != content.items.by_id.end() ? item->second.name() : "";
    }
    case Event::Kind::kTake:
    case Event::Kind::kTurnIn: {
      const auto quest = content.quests.find(event.quest);
      return quest != content.quests.end() ? quest->second.name() : "";
    }
    case Event::Kind::kLevel:
    case Event::Kind::kHeal:
      return "";
  }
  return "";
}

}  // namespace

std::string Row(const Event& event, const Content& content,
                std::string_view world, absl::Time time) {
  const nlohmann::json row = {
      // To the millisecond, in UTC, as ClickHouse reads a DateTime64(3).
      {"time",
       absl::FormatTime("%Y-%m-%d %H:%M:%E3S", time, absl::UTCTimeZone())},
      {"world", world},
      {"land", event.land >= 0 && event.land < static_cast<int>(kLands.size())
                   ? kLands[event.land].name
                   : ""},
      {"kind", Name(event.kind)},
      {"what", What(event, content)},
      {"player", event.player},
      {"x", event.x},
      {"z", event.z},
      {"monster", event.monster},
      {"item", event.item},
      {"count", event.count},
      {"quest", event.quest},
      {"level", event.level},
  };
  return row.dump();
}

bool ForAnalytics(Event::Kind kind) {
  switch (kind) {
    case Event::Kind::kKill:
    case Event::Kind::kDeath:
    case Event::Kind::kLoot:
    case Event::Kind::kPrize:
    case Event::Kind::kLevel:
    case Event::Kind::kTake:
    case Event::Kind::kTurnIn:
    case Event::Kind::kUse:
      return true;
    case Event::Kind::kHit:
    case Event::Kind::kHurt:
    case Event::Kind::kHeal:
      return false;
  }
  return false;
}

absl::Status Events::Add(const std::vector<std::string>& rows) {
  std::vector<std::vector<std::string>> commands;
  commands.reserve(rows.size());
  for (const std::string& row : rows) {
    // With ~, Redis trims the stream a block at a time, which costs less than
    // trimming it to the entry.
    commands.push_back({"XADD", kEventStream, "MAXLEN", "~",
                        std::to_string(kKeep), "*", "row", row});
  }
  return redis_.Pipeline(commands).status();
}

}  // namespace nanommo
