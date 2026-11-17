#ifndef NANOMMO_SERVER_WEATHER_H_
#define NANOMMO_SERVER_WEATHER_H_

#include <cstdint>

#include "protocol/nanommo.pb.h"

namespace nanommo {

/// From one midnight to the next, in milliseconds of the wall clock.
inline constexpr int64_t kDayMs = 40 * 60 * 1000;

/// The weather comes in spells, at most one in each slot of this many
/// milliseconds, starting at the slot's start and lasting from the shortest
/// to the whole slot. The rest of the slot is clear.
inline constexpr int64_t kSlotMs = 15 * 60 * 1000;
inline constexpr int64_t kShortestSpellMs = 6 * 60 * 1000;

/// How long a spell takes to come, and to go, in milliseconds.
inline constexpr int64_t kRampMs = 90 * 1000;

/// The sky over a land, by its place in the lands, at a moment of the wall
/// clock, in milliseconds since 1970. It's worked out from those two alone,
/// so every server that runs the land agrees, even after a restart, and
/// there's nothing to save.
protocol::Sky SkyAt(int land, int64_t unix_ms);

/// Now, by the wall clock, in milliseconds since 1970.
int64_t UnixMs();

}  // namespace nanommo

#endif  // NANOMMO_SERVER_WEATHER_H_
