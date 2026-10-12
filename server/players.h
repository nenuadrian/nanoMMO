#ifndef NANOMMO_SERVER_PLAYERS_H_
#define NANOMMO_SERVER_PLAYERS_H_

#include <cstdint>
#include <map>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "server/database.h"
#include "server/redis.h"
#include "server/world.h"

namespace nanommo {

/// What we keep of a player from one session to the next.
struct Progress {
  float x = 0.0f;
  float z = 0.0f;
  uint32_t experience = 0;

  bool operator==(const Progress&) const = default;
};

class Players {
 public:
  Players(Database& database, Redis& redis)
      : database_(database), redis_(redis) {}

  /// From MySQL, with what Redis has on top, as that's newer.
  absl::StatusOr<Player> Load(uint32_t account_id);

  /// Into Redis, all of them in a single round trip.
  absl::Status Save(const std::map<uint32_t, Progress>& changed);

 private:
  Database& database_;
  Redis& redis_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_PLAYERS_H_
