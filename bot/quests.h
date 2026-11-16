#ifndef NANOMMO_BOT_QUESTS_H_
#define NANOMMO_BOT_QUESTS_H_

#include <cstdint>
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
  /// The monster a hunt goes after, by its id, so that the bot can tell once
  /// it's dead.
  uint32_t monster = 0;
  /// Who to, and which quest, unless it's a hunt.
  protocol::Quest quest;
};

/// How near a living boss has to be for a bot to join its fight, in metres:
/// from the middle of its land, the Bone Colossus is.
inline constexpr float kJoinTheFight = 60.0f;

/// What a bot does next, from what it sees: goes after the nearest boss within
/// kJoinTheFight, as players flock to one, or else, for its quests, turns in
/// a quest it's done, if its giver's here, or goes after the nearest monster a
/// quest it's on asks for, or takes a quest it hasn't done from an NPC here
/// who gives it. Nothing, if none of them is here.
std::optional<Errand> NextErrand(const Content& content,
                                 const protocol::Quests& quests,
                                 const protocol::World& seen, float x, float z);

/// Whether the world shows the monster dead, by its id. One the world doesn't
/// have isn't, as it may be out of sight.
bool Dead(const protocol::World& seen, uint32_t monster);

}  // namespace nanommo

#endif  // NANOMMO_BOT_QUESTS_H_
