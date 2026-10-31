#ifndef NANOMMO_TERRAIN_PATHS_H_
#define NANOMMO_TERRAIN_PATHS_H_

#include <cstdint>
#include <vector>

#include "terrain/lands.h"
#include "terrain/terrain.h"

namespace nanommo {

/// The ways across a land: which of its metre squares a player can walk on,
/// and the shortest way from one place to another over them, which A* finds.
/// A world finds a way each time a player asks to walk somewhere, between
/// ticks, so a search looks at kMostSearched squares at most, and then goes as
/// near as it got.
class Paths {
 public:
  /// In metres along each side.
  static constexpr float kSquare = 1.0f;

  /// A little under a sixth of the island's squares.
  static constexpr int kMostSearched = 40000;

  explicit Paths(const Terrain& terrain);

  /// The way from one place to the other: the places to walk to straight,
  /// one after the other, the last of them to, or the place nearest it that
  /// the search reached. Empty when there's no way to go: to isn't somewhere
  /// a player can stand, or from is off the land.
  std::vector<Place> Find(Place from, Place to);

  /// Whether a straight walk from one place to the other stays on squares a
  /// player can walk on.
  bool Clear(Place from, Place to) const;

  /// Whether the square the place is in is one a player can walk on.
  bool Open(Place place) const;

  /// How many squares the last search looked at.
  int searched() const { return searched_; }

 private:
  /// The square the place is in, by its index, or -1 off the land.
  int SquareOf(Place place) const;
  Place Middle(int square) const;
  /// Along the shortest way there could be, around nothing.
  float Estimate(int square, int goal) const;
  /// The squares, as places, cut down to the places where the way turns, so
  /// that each is in sight of the one before.
  std::vector<Place> Straighten(Place from,
                                const std::vector<Place>& squares) const;

  int size_ = 0;
  float half_ = 0.0f;
  std::vector<bool> open_;
  // What the searches work with, kept from one to the next, so that a search
  // doesn't clear or allocate a quarter of a million of each: a square's cost
  // and where it was reached from count only while its stamp is the number of
  // the search going on.
  std::vector<float> cost_;
  std::vector<int> came_from_;
  std::vector<uint32_t> stamp_;
  uint32_t search_ = 0;
  int searched_ = 0;
};

}  // namespace nanommo

#endif  // NANOMMO_TERRAIN_PATHS_H_
