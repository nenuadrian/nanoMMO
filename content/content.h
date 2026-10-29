#ifndef NANOMMO_CONTENT_CONTENT_H_
#define NANOMMO_CONTENT_CONTENT_H_

#include <cstdint>
#include <map>
#include <string>

#include "absl/status/statusor.h"
#include "content/figures.h"
#include "content/items.h"
#include "content/monsters.h"
#include "content/npcs.h"
#include "content/quests.h"
#include "content/skills.h"

namespace nanommo {

/// What the game is made of, which the world servers and the clients both
/// read from the content folder.
struct Content {
  figures::Figures figures;
  ItemTable items;
  MonsterTable monsters;
  NpcTable npcs;
  /// By id.
  std::map<uint32_t, quests::Quest> quests;
  SkillTable skills;
};

/// Reads each file in the folder, or says which file is wrong, and where.
absl::StatusOr<Content> ReadContent(const std::string& folder);

/// What the whole folder can tell is wrong, and no single file can: a monster
/// that drops an item the items file doesn't have, a trainer that teaches a
/// skill the skills file doesn't have, or a quest whose giver, monster or
/// prize isn't in its file.
absl::Status CheckContent(const Content& content);

/// A fingerprint of what the clients read, so that a client can tell whether
/// a world has its content. What only the server reads, such as a weapon's
/// damage, stays out, so that changing it doesn't need a new client.
uint64_t ClientHash(const Content& content);

}  // namespace nanommo

#endif  // NANOMMO_CONTENT_CONTENT_H_
