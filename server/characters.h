#ifndef NANOMMO_SERVER_CHARACTERS_H_
#define NANOMMO_SERVER_CHARACTERS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "protocol/nanommo.pb.h"
#include "server/database.h"

namespace nanommo {

/// An account's characters, which the login server lists, makes and deletes.
/// Each is in one world, by its id, with a name nobody else in that world has.
/// What a world saves of them, it saves by that id (Players).
class Characters {
 public:
  /// How many characters an account can have in each world.
  static constexpr int kPerWorld = 4;

  explicit Characters(Database& database) : database_(database) {}

  /// All the account's characters, in all the worlds, the oldest first, with
  /// what they wear, from what MySQL has: what a world has saved in Redis and
  /// not yet flushed shows a few seconds later.
  absl::StatusOr<std::vector<protocol::CharacterListing>> List(
      uint32_t account);

  /// A new character, which gets the starting kit as it first enters its
  /// world. An InvalidArgument error says what's wrong with the name, an
  /// AlreadyExists error that it's taken in the world, and a
  /// ResourceExhausted one that the account has kPerWorld there already.
  absl::StatusOr<uint32_t> Create(uint32_t account, const std::string& world,
                                  const std::string& name);

  /// Deletes the account's character, with its skills and its bag, or says
  /// NotFound when the account has no character with that id.
  absl::Status Delete(uint32_t account, uint32_t id);

  /// The world the account's character is in, or NotFound when the account
  /// has no character with that id.
  absl::StatusOr<std::string> WorldOf(uint32_t account, uint32_t id);

 private:
  Database& database_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_CHARACTERS_H_
