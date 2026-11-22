#ifndef NANOMMO_BOT_WAR_H_
#define NANOMMO_BOT_WAR_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "protocol/nanommo.pb.h"

namespace nanommo {

/// A bot at war is in one of two guilds, and fights the other's players.
struct Side {
  std::string guild;
  std::string enemy;
  /// For the first bot of each side, which founds the guild and leads it, the
  /// rest of the side, by their names, whom it asks in. Empty for the rest,
  /// who say yes when it asks.
  std::vector<std::string> recruits;
  /// Whether the bot is the first of its side, which leads it even when
  /// nobody else is on it.
  bool leads = false;
  /// How many bots are on the side, its leader with them.
  int size = 1;
  /// Where the bot waits for the rest of its side to join, before the charge:
  /// its place in its side's camp, either side of where new players come in.
  float camp_x = 0.0f;
  float camp_z = 0.0f;
};

/// What the side's leader does about its guild next, one thing at a time:
/// founds it, asks the enemy to war until they're at war, and then asks in
/// the first of its side who isn't in it yet, from the recruit after the one
/// it asked last, next, round the list. Nothing for the rest of the side, or
/// once everyone's in.
std::optional<protocol::GuildAction> Enlist(const Side& side,
                                            const protocol::Guild& guild,
                                            std::size_t& next);

/// The nearest player of the enemy guild the bot at x and z can see, alive,
/// by their id, or 0 for none.
uint32_t NearestEnemy(const Side& side, const protocol::World& seen, float x,
                      float z);

}  // namespace nanommo

#endif  // NANOMMO_BOT_WAR_H_
