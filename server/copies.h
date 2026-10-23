#ifndef NANOMMO_SERVER_COPIES_H_
#define NANOMMO_SERVER_COPIES_H_

#include <chrono>
#include <cstdint>
#include <map>

#include "content/content.h"
#include "server/interest.h"
#include "server/world.h"

namespace nanommo {

/// A land, running: a World of it, with its NPCs in their cells.
struct Copy {
  /// By its place in kLands.
  int land = 0;
  World world;
  Cells npc_cells;
};

/// The world's lands, as copies. Each land has one, which everyone in the
/// world shares. Each copy has an id, so that a land can have more than one.
class Copies {
 public:
  /// The world's seed makes each land's terrain, and the content fills them.
  Copies(uint32_t seed, const Content& content);

  /// Into their land's copy.
  void Join(uint32_t id, Player player);

  void Leave(uint32_t id);

  /// The copy the player's in, or nullptr.
  Copy* Of(uint32_t id);

  /// Steps the copies, and takes players through the gates.
  void Step(std::chrono::nanoseconds time);

  /// By their ids.
  const std::map<uint32_t, Copy>& all() const { return copies_; }

  /// In all the copies.
  int players() const;

 private:
  /// The copy of the land.
  uint32_t For(int land) const;
  void Travel();

  std::map<uint32_t, Copy> copies_;
  uint32_t next_ = 1;
  /// The copy each player's in, by their ids.
  std::map<uint32_t, uint32_t> where_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_COPIES_H_
