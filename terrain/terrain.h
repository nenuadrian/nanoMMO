#ifndef NANOMMO_TERRAIN_TERRAIN_H_
#define NANOMMO_TERRAIN_TERRAIN_H_

#include <cstdint>
#include <vector>

namespace nanommo {

struct Tree {
  float x = 0.0f;
  float z = 0.0f;
  /// In radians, around the vertical.
  float yaw = 0.0f;
  /// How much bigger or smaller than the model it is.
  float scale = 1.0f;
};

/// The ground everyone walks on. The server and the clients each make it
/// from the same seed, so only the seed goes over the network.
class Terrain {
 public:
  /// In metres. Each cell of the grid is two triangles.
  static constexpr float kCell = 2.0f;

  static constexpr float kSeaLevel = 0.0f;

  /// Players wade into the sea up to their knees.
  static constexpr float kWading = 0.4f;

  /// The trees grow on a grid of squares this size, at most one in each.
  static constexpr float kTreeSpacing = 4.0f;

  /// How close to the middle of a trunk a player can stand.
  static constexpr float kTrunk = 0.5f;

  /// The board we had: flat, twelve metres across, for the tests.
  Terrain();

  /// An island, 512 metres across, with the sea all around it.
  explicit Terrain(uint32_t seed);

  /// On the triangles between the grid's corners, as the client draws them.
  float Height(float x, float z) const;

  /// Whether a player can stand there: on the grid, out of the sea, and not
  /// in a tree.
  bool Walkable(float x, float z) const;

  const std::vector<Tree>& trees() const { return trees_; }

  /// The height at a corner of the grid, from 0 to cells() along each side.
  float Corner(int i, int j) const { return corners_[j * (cells_ + 1) + i]; }

  uint32_t seed() const { return seed_; }

  /// Along each side.
  int cells() const { return cells_; }

  /// From the middle to each side of the grid.
  float half_size() const { return cells_ * kCell / 2.0f; }

  /// Half a metre short of each side, so that players stay on the grid.
  float edge() const { return half_size() - 0.5f; }

 private:
  void Plant();
  bool InATree(float x, float z) const;

  uint32_t seed_ = 0;
  int cells_ = 0;
  std::vector<float> corners_;
  std::vector<Tree> trees_;
  // For each square of the trees' grid, the tree in it, or -1.
  std::vector<int> tree_in_;
  int tree_squares_ = 0;
};

}  // namespace nanommo

#endif  // NANOMMO_TERRAIN_TERRAIN_H_
