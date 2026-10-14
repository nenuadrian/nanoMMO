#include "server/interest.h"

#include <cmath>
#include <map>
#include <vector>

namespace nanommo {

Cell CellOf(float x, float z) {
  return {.x = static_cast<int>(std::floor(x / kCellSize)),
          .z = static_cast<int>(std::floor(z / kCellSize))};
}

std::map<Cell, View> Views(const std::map<uint32_t, Player>& players,
                           const std::map<uint32_t, Monster>& monsters) {
  std::map<Cell, std::vector<uint32_t>> players_in;
  for (const auto& [id, player] : players) {
    players_in[CellOf(player.x, player.z)].push_back(id);
  }
  std::map<Cell, std::vector<uint32_t>> monsters_in;
  for (const auto& [id, monster] : monsters) {
    monsters_in[CellOf(monster.x, monster.z)].push_back(id);
  }

  std::map<Cell, View> views;
  for (const auto& [cell, here] : players_in) {
    View& view = views[cell];
    view.here = here;
    for (int dz = -kInterestCells; dz <= kInterestCells; ++dz) {
      for (int dx = -kInterestCells; dx <= kInterestCells; ++dx) {
        const Cell near{.x = cell.x + dx, .z = cell.z + dz};
        if (const auto found = players_in.find(near);
            found != players_in.end()) {
          view.players.insert(view.players.end(), found->second.begin(),
                              found->second.end());
        }
        if (const auto found = monsters_in.find(near);
            found != monsters_in.end()) {
          view.monsters.insert(view.monsters.end(), found->second.begin(),
                               found->second.end());
        }
      }
    }
  }
  return views;
}

}  // namespace nanommo
