#ifndef NANOMMO_SERVER_EVENTS_H_
#define NANOMMO_SERVER_EVENTS_H_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "content/content.h"
#include "server/redis.h"
#include "server/world.h"

namespace nanommo {

/// The Redis stream the worlds add the game's events to, and the recorder
/// takes them from, into ClickHouse.
inline constexpr char kEventStream[] = "events";

/// An event as a row for ClickHouse: a line of JSON, as its JSONEachRow format
/// reads them, with when it happened and in which world, and the name of what
/// it's about, from the content.
std::string Row(const Event& event, const Content& content,
                std::string_view world, absl::Time time);

/// Where the worlds put the game's events, a Redis stream, so that a tick
/// never waits on the analytics.
class Events {
 public:
  /// The stream keeps about the newest this many, so that it can't fill
  /// Redis's memory while the recorder's down.
  static constexpr int kKeep = 1'000'000;

  explicit Events(Redis& redis) : redis_(redis) {}

  /// Adds the rows to the stream, all of them in a single round trip.
  absl::Status Add(const std::vector<std::string>& rows);

 private:
  Redis& redis_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_EVENTS_H_
