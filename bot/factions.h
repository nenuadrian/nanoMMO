#ifndef NANOMMO_BOT_FACTIONS_H_
#define NANOMMO_BOT_FACTIONS_H_

#include <cstdint>
#include <optional>

#include "bot/quests.h"
#include "content/content.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// The faction a bot swears to, by its id in the factions file: each in turn,
/// by the bot's seed, so that a crowd of bots wears all their colours. 0 for
/// content without any.
uint32_t FactionFor(const Content& content, uint32_t seed);

/// A trip to the faction's herald, to swear to it, where the bot sees them,
/// or, in the land the bot's in, by its place in kLands, to where the NPCs
/// file stands them, with no herald to swear at until it sees them there.
/// Nothing in a land without them.
std::optional<Errand> Oath(const Content& content, uint32_t faction,
                           const protocol::World& seen, int land);

/// Whether the world shows the bot sworn to a faction, which comes with its
/// name, once a second.
bool Sworn(const protocol::World& seen);

}  // namespace nanommo

#endif  // NANOMMO_BOT_FACTIONS_H_
