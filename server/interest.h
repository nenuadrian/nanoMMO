#ifndef NANOMMO_SERVER_INTEREST_H_
#define NANOMMO_SERVER_INTEREST_H_

#include <compare>
#include <cstdint>
#include <map>
#include <vector>

#include "server/world.h"

namespace nanommo {

/// In metres, like the client's chunks.
constexpr float kCellSize = 32.0f;

/// Cells each way, so that a player gets everyone within 64 metres.
constexpr int kInterestCells = 2;

struct Cell {
  int x = 0;
  int z = 0;

  auto operator<=>(const Cell&) const = default;
};

Cell CellOf(float x, float z);

/// The ids of what's in each cell.
using Cells = std::map<Cell, std::vector<uint32_t>>;

template <typename Unit>
Cells CellsOf(const std::map<uint32_t, Unit>& units) {
  Cells cells;
  for (const auto& [id, unit] : units) {
    cells[CellOf(unit.x, unit.z)].push_back(id);
  }
  return cells;
}

/// What the players in a cell get: the players, monsters and NPCs near it.
struct View {
  std::vector<uint32_t> here;
  std::vector<uint32_t> players;
  std::vector<uint32_t> monsters;
  std::vector<uint32_t> npcs;
};

/// Players and monsters move, so they go into their cells each time. NPCs
/// stand still, so the server puts them in theirs once, when they spawn.
std::map<Cell, View> Views(const std::map<uint32_t, Player>& players,
                           const std::map<uint32_t, Monster>& monsters,
                           const Cells& npcs);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_INTEREST_H_
