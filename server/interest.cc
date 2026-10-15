#include "server/interest.h"

#include <cmath>
#include <map>
#include <vector>

namespace nanommo {

namespace {

void Gather(const Cells& cells, Cell cell, std::vector<uint32_t>& ids) {
  if (const auto found = cells.find(cell); found != cells.end()) {
    ids.insert(ids.end(), found->second.begin(), found->second.end());
  }
}

}  // namespace

Cell CellOf(float x, float z) {
  return {.x = static_cast<int>(std::floor(x / kCellSize)),
          .z = static_cast<int>(std::floor(z / kCellSize))};
}

std::map<Cell, View> Views(const std::map<uint32_t, Player>& players,
                           const std::map<uint32_t, Monster>& monsters,
                           const Cells& npcs) {
  const Cells players_in = CellsOf(players);
  const Cells monsters_in = CellsOf(monsters);

  std::map<Cell, View> views;
  for (const auto& [cell, here] : players_in) {
    View& view = views[cell];
    view.here = here;
    for (int dz = -kInterestCells; dz <= kInterestCells; ++dz) {
      for (int dx = -kInterestCells; dx <= kInterestCells; ++dx) {
        const Cell near{.x = cell.x + dx, .z = cell.z + dz};
        Gather(players_in, near, view.players);
        Gather(monsters_in, near, view.monsters);
        Gather(npcs, near, view.npcs);
      }
    }
  }
  return views;
}

}  // namespace nanommo
