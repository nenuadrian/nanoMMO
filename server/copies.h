#ifndef NANOMMO_SERVER_COPIES_H_
#define NANOMMO_SERVER_COPIES_H_

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "content/content.h"
#include "server/interest.h"
#include "server/world.h"
#include "terrain/terrain.h"

namespace nanommo {

/// A land, running: a World of it, with its NPCs in their cells.
struct Copy {
  /// By its place in kLands.
  int land = 0;
  World world;
  Cells npc_cells;
  /// Whose copy of a dungeon it is, by their id. Nobody owns an open land's
  /// copy, which everyone in the world shares.
  uint32_t owner = 0;
  /// How long it's had nobody in it.
  std::chrono::nanoseconds empty{0};
};

/// The world's lands, as copies. Each open land has one from the start, and a
/// dungeon has one for each player who goes in, from when they do.
class Copies {
 public:
  /// After its last player leaves, a dungeon's copy stops, and waits this long
  /// for them before it goes, so that someone who steps out finds it as they
  /// left it.
  static constexpr std::chrono::minutes kEmptyLife{5};

  /// The world's seed makes each land's terrain, and the content fills them.
  Copies(uint32_t seed, Content content);

  /// Into their land's copy: the open land's, or for a dungeon, their own.
  void Join(uint32_t id, Player player);

  void Leave(uint32_t id);

  /// The copy the player's in, or nullptr.
  Copy* Of(uint32_t id);

  /// Steps the copies with players in them, and the open lands' always, takes
  /// players through the gates, and drops the dungeons' copies that have been
  /// empty for kEmptyLife.
  void Step(std::chrono::nanoseconds time);

  /// By their ids.
  const std::map<uint32_t, Copy>& all() const { return copies_; }

  /// What all of them are made of, which doesn't change as the game runs.
  const Content& content() const { return *content_; }

  /// The lines each copy has for its players since the last time.
  std::vector<Notice> TakeNotices();

  /// What's happened in all of them since the last time.
  std::vector<Event> TakeEvents();

  /// In all the copies.
  int players() const;

 private:
  /// The copy of the land for the player, which for a dungeon it makes if
  /// they haven't one.
  uint32_t For(int land, uint32_t id);
  void Travel();

  /// One copy, which each copy of each land shares.
  std::shared_ptr<const Content> content_;
  /// Each land's, made once, which each of its copies copies.
  std::vector<Terrain> terrains_;
  std::map<uint32_t, Copy> copies_;
  uint32_t next_ = 1;
  /// The copy each player's in, by their ids.
  std::map<uint32_t, uint32_t> where_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_COPIES_H_
