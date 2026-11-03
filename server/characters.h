#ifndef NANOMMO_SERVER_CHARACTERS_H_
#define NANOMMO_SERVER_CHARACTERS_H_

#include <cstdint>
#include <string>

#include "absl/status/statusor.h"
#include "server/database.h"

namespace nanommo {

/// An account's characters. Each is in one world, by its id, with a name
/// nobody else in that world has. What a world saves of them, it saves by that
/// id (Players).
class Characters {
 public:
  /// How many characters an account can have in each world.
  static constexpr int kPerWorld = 4;

  explicit Characters(Database& database) : database_(database) {}

  /// A new character, which gets the starting kit as it first enters its
  /// world. An InvalidArgument error says what's wrong with the name, an
  /// AlreadyExists error that it's taken in the world, and a
  /// ResourceExhausted one that the account has kPerWorld there already.
  absl::StatusOr<uint32_t> Create(uint32_t account, const std::string& world,
                                  const std::string& name);

 private:
  Database& database_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_CHARACTERS_H_
