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

/// What the players in a cell get: the players and the monsters near it.
struct View {
  std::vector<uint32_t> here;
  std::vector<uint32_t> players;
  std::vector<uint32_t> monsters;
};

std::map<Cell, View> Views(const std::map<uint32_t, Player>& players,
                           const std::map<uint32_t, Monster>& monsters);

}  // namespace nanommo

#endif  // NANOMMO_SERVER_INTEREST_H_
