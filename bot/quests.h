#ifndef NANOMMO_BOT_QUESTS_H_
#define NANOMMO_BOT_QUESTS_H_

#include <optional>

#include "content/content.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// A trip a bot makes for its quests: to a monster a quest asks for, to hit
/// it, or to an NPC, to take a quest or turn one in.
struct Errand {
  enum class Kind { kHunt, kTake, kTurnIn };
  Kind kind = Kind::kHunt;
  float x = 0.0f;
  float z = 0.0f;
  /// Who to, and which quest, unless it's a hunt.
  protocol::Quest quest;
};

/// What a bot does next for its quests, from what it sees: turns in a quest
/// it's done, if its giver's here, or goes after the nearest monster a quest
/// it's on asks for, or takes a quest it hasn't done from an NPC here who
/// gives it. Nothing, if none of them is here.
std::optional<Errand> NextErrand(const Content& content,
                                 const protocol::Quests& quests,
                                 const protocol::World& seen, float x, float z);

}  // namespace nanommo

#endif  // NANOMMO_BOT_QUESTS_H_
