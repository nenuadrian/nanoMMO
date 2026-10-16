#ifndef NANOMMO_CONTENT_CONTENT_H_
#define NANOMMO_CONTENT_CONTENT_H_

#include <cstdint>
#include <string>

#include "absl/status/statusor.h"
#include "content/items.h"
#include "content/monsters.h"
#include "content/npcs.h"

namespace nanommo {

/// What the game is made of, which the world servers and the clients both
/// read from the content folder.
struct Content {
  ItemTable items;
  MonsterTable monsters;
  NpcTable npcs;
};

/// Reads each file in the folder, or says which file is wrong, and where.
absl::StatusOr<Content> ReadContent(const std::string& folder);

/// A fingerprint of what the clients read, so that a client can tell whether
/// a world has its content. What only the server reads, such as a weapon's
/// damage, stays out, so that changing it doesn't need a new client.
uint64_t ClientHash(const Content& content);

}  // namespace nanommo

#endif  // NANOMMO_CONTENT_CONTENT_H_
