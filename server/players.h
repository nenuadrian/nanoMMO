#ifndef NANOMMO_SERVER_PLAYERS_H_
#define NANOMMO_SERVER_PLAYERS_H_

#include <cstdint>
#include <map>
#include <string>

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
  int health = 0;
  int land = 0;
  uint32_t weapon = 0;

  bool operator==(const Progress&) const = default;
};

class Players {
 public:
  static constexpr int kBatch = 500;

  Players(Database& database, Redis& redis)
      : database_(database), redis_(redis) {}

  /// The account's player in the world, from MySQL, with what Redis has on
  /// top, as that's newer.
  absl::StatusOr<Player> Load(const std::string& world, uint32_t account_id);

  /// Into Redis, all of them in a single round trip, and marked unsaved.
  absl::Status Save(const std::string& world,
                    const std::map<uint32_t, Progress>& changed);

  /// Takes the unsaved players of all the worlds from Redis to MySQL, a batch
  /// at a time, and returns how many it saved.
  absl::StatusOr<int> Flush();

 private:
  Database& database_;
  Redis& redis_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_PLAYERS_H_
