#ifndef NANOMMO_SERVER_PLAYERS_H_
#define NANOMMO_SERVER_PLAYERS_H_

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "server/database.h"
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
  explicit Players(Database& database) : database_(database) {}

  absl::StatusOr<Player> Load(uint32_t account_id);

  absl::Status Save(uint32_t account_id, const Progress& progress);

 private:
  Database& database_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_PLAYERS_H_
