#include "server/history.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <iterator>
#include <optional>

namespace nanommo {

void History::Record(uint32_t id, std::chrono::nanoseconds time, Place place) {
  std::deque<Moment>& moments = moments_[id];
  moments.push_back({.time = time, .place = place});
  while (moments.front().time < time - kKept) {
    moments.pop_front();
  }
}

std::optional<Place> History::At(uint32_t id,
                                 std::chrono::nanoseconds time) const {
  const auto found = moments_.find(id);
  if (found == moments_.end()) {
    return std::nullopt;
  }
  const std::deque<Moment>& moments = found->second;
  // The first place after the time, and the one before it.
  const auto after = std::ranges::upper_bound(moments, time, {}, &Moment::time);
  if (after == moments.begin()) {
    return moments.front().place;
  }
  if (after == moments.end()) {
    return moments.back().place;
  }
  const Moment& from = *std::prev(after);
  const float share = std::chrono::duration<float>(time - from.time) /
                      std::chrono::duration<float>(after->time - from.time);
  return Place{.x = from.place.x + (after->place.x - from.place.x) * share,
               .z = from.place.z + (after->place.z - from.place.z) * share};
}

void History::Leave(uint32_t id) { moments_.erase(id); }

}  // namespace nanommo
