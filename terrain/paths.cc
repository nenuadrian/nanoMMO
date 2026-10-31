#include "terrain/paths.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <numbers>
#include <queue>
#include <utility>
#include <vector>

namespace nanommo {

namespace {

// A square is open when a player can stand all over it: on its middle, its
// corners and the middles of its sides, as shares of a square from its middle.
// Then a straight walk over open squares has ground under it all the way, and
// a way past a tree keeps clear of its trunk.
constexpr std::array<float, 3> kOver = {-0.5f, 0.0f, 0.5f};

// A straight walk is checked at points this far apart.
constexpr float kLook = 0.25f;

constexpr float kDiagonal = std::numbers::sqrt2_v<float>;

}  // namespace

Paths::Paths(const Terrain& terrain)
    : size_(static_cast<int>(std::ceil(2.0f * terrain.half_size() / kSquare))),
      half_(terrain.half_size()) {
  const int squares = size_ * size_;
  open_.resize(squares);
  for (int square = 0; square < squares; ++square) {
    const Place middle = Middle(square);
    bool open = true;
    for (const float down : kOver) {
      for (const float across : kOver) {
        open = open && terrain.Walkable(middle.x + across * kSquare,
                                        middle.z + down * kSquare);
      }
    }
    open_[square] = open;
  }
  cost_.resize(squares);
  came_from_.resize(squares);
  stamp_.assign(squares, 0);
}

int Paths::SquareOf(Place place) const {
  const int i = static_cast<int>(std::floor((place.x + half_) / kSquare));
  const int j = static_cast<int>(std::floor((place.z + half_) / kSquare));
  if (i < 0 || j < 0 || i >= size_ || j >= size_) {
    return -1;
  }
  return j * size_ + i;
}

Place Paths::Middle(int square) const {
  const int across = square % size_;
  const int down = square / size_;
  return {.x = (static_cast<float>(across) + 0.5f) * kSquare - half_,
          .z = (static_cast<float>(down) + 0.5f) * kSquare - half_};
}

bool Paths::Open(Place place) const {
  const int square = SquareOf(place);
  return square >= 0 && open_[square];
}

bool Paths::Clear(Place from, Place to) const {
  const float length = std::hypot(to.x - from.x, to.z - from.z);
  const int points = static_cast<int>(std::ceil(length / kLook));
  for (int point = 1; point <= points; ++point) {
    const float share = static_cast<float>(point) / static_cast<float>(points);
    if (!Open({.x = from.x + (to.x - from.x) * share,
               .z = from.z + (to.z - from.z) * share})) {
      return false;
    }
  }
  return true;
}

// Octile distance: diagonally as far as it can, then straight.
float Paths::Estimate(int square, int goal) const {
  const int square_down = square / size_;
  const int goal_down = goal / size_;
  const auto across =
      static_cast<float>(std::abs(square % size_ - goal % size_));
  const auto down = static_cast<float>(std::abs(square_down - goal_down));
  return (std::max(across, down) +
          (kDiagonal - 1.0f) * std::min(across, down)) *
         kSquare;
}

std::vector<Place> Paths::Find(Place from, Place to) {
  const int start = SquareOf(from);
  const int goal = SquareOf(to);
  searched_ = 0;
  if (start < 0 || goal < 0 || !open_[goal]) {
    return {};
  }
  // Most walks have nothing in the way.
  if (Clear(from, to)) {
    return {to};
  }
  ++search_;
  // A square can go into the queue more than once, as a shorter way to it
  // turns up; the longer ones come out later, and are skipped.
  using Entry = std::pair<float, int>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> frontier;
  stamp_[start] = search_;
  cost_[start] = 0.0f;
  came_from_[start] = -1;
  frontier.push({Estimate(start, goal), start});
  int nearest = start;
  float nearest_left = Estimate(start, goal);
  while (!frontier.empty() && searched_ < kMostSearched) {
    const auto [estimate, square] = frontier.top();
    frontier.pop();
    const float left = Estimate(square, goal);
    if (estimate > cost_[square] + left + 1e-3f) {
      continue;
    }
    ++searched_;
    if (left < nearest_left) {
      nearest = square;
      nearest_left = left;
    }
    if (square == goal) {
      break;
    }
    const int i = square % size_;
    const int j = square / size_;
    for (int dj = -1; dj <= 1; ++dj) {
      for (int di = -1; di <= 1; ++di) {
        const int ni = i + di;
        const int nj = j + dj;
        if ((di == 0 && dj == 0) || ni < 0 || nj < 0 || ni >= size_ ||
            nj >= size_) {
          continue;
        }
        const int next = nj * size_ + ni;
        // Not between two squares that touch at a corner only, which would
        // cut through the corner of what's in the way.
        const bool diagonal = di != 0 && dj != 0;
        if (!open_[next] ||
            (diagonal && (!open_[j * size_ + ni] || !open_[nj * size_ + i]))) {
          continue;
        }
        const float cost =
            cost_[square] + (diagonal ? kDiagonal : 1.0f) * kSquare;
        if (stamp_[next] != search_ || cost < cost_[next]) {
          stamp_[next] = search_;
          cost_[next] = cost;
          came_from_[next] = square;
          frontier.push({cost + Estimate(next, goal), next});
        }
      }
    }
  }
  std::vector<Place> squares;
  for (int square = nearest; square != start; square = came_from_[square]) {
    squares.push_back(Middle(square));
  }
  std::ranges::reverse(squares);
  if (nearest == goal) {
    squares.push_back(to);
  }
  return Straighten(from, squares);
}

// From where we are, on to the farthest square still in sight, and again from
// there: the way turns only where something's in the way.
std::vector<Place> Paths::Straighten(Place from,
                                     const std::vector<Place>& squares) const {
  std::vector<Place> way;
  Place at = from;
  std::size_t next = 0;
  while (next < squares.size()) {
    std::size_t farthest = next;
    while (farthest + 1 < squares.size() && Clear(at, squares[farthest + 1])) {
      ++farthest;
    }
    way.push_back(squares[farthest]);
    at = squares[farthest];
    next = farthest + 1;
  }
  return way;
}

}  // namespace nanommo
