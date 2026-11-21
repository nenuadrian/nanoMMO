#ifndef NANOMMO_SERVER_SHOWN_H_
#define NANOMMO_SERVER_SHOWN_H_

#include <cstdint>
#include <optional>

#include "protocol/nanommo.pb.h"
#include "server/world.h"

namespace nanommo {

// What each player is shown of a world, as the protocol has it, whoever runs
// the world: a world server, or a client that plays on its own.

protocol::Motion ToProtocol(Motion motion);
protocol::Player ToProtocol(uint32_t id, const Player& player);
protocol::Monster ToProtocol(uint32_t id, const Monster& monster);
protocol::Npc ToProtocol(uint32_t id, const Npc& npc);

/// What a player's own client is told about them each tick, beside the world
/// near them: who they are, their experience and the next level's, their
/// skills and how long each has left to cool down, how long until they rise
/// again, what they're making and how far it's got, the land and its ground's
/// seed, the last of their numbered inputs applied, if any, and where they are
/// with it applied.
protocol::World YouIn(uint32_t id, const Player& player, uint32_t seed,
                      int land, std::optional<uint32_t> acked);

/// Their bag, and their quests, each as it goes when it's changed.
protocol::World BagOf(const Player& player);
protocol::World QuestsOf(const Player& player);

/// The event as the players near it get it, or nothing for a kind they don't
/// see, such as a quest taken.
std::optional<protocol::Event> ForPlayers(const Event& event);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_SHOWN_H_
