#include "client/interpolation.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <utility>

namespace nanommo {

namespace {

// A second of worlds, which is plenty to draw from, and to learn the clocks
// from.
constexpr std::size_t kMostSnapshots = 20;

// From a to b, by the share of the way from one to the other.
float Mix(float a, float b, float share) { return a + (b - a) * share; }

// Each unit in the newest world but you, where it was between the two
// snapshots, by the share of the way from one to the other.
template <typename Unit, typename Places>
void Between(google::protobuf::RepeatedPtrField<Unit>& units, uint32_t you,
             const Places& before, const Places& after, float share) {
  for (Unit& unit : units) {
    const auto from = before.find(unit.id());
    const auto to = after.find(unit.id());
    if (unit.id() == you || from == before.end() || to == after.end()) {
      continue;
    }
    unit.set_x(Mix(from->second.x, to->second.x, share));
    unit.set_z(Mix(from->second.z, to->second.z, share));
  }
}

}  // namespace

void Interpolation::Add(const protocol::World& world, double now) {
  const double sent = static_cast<double>(world.sent()) / 1e6;
  // A part of an older world came late, and is no use now.
  if (!snapshots_.empty() && sent < snapshots_.back().sent) {
    return;
  }
  if (snapshots_.empty() || sent > snapshots_.back().sent) {
    snapshots_.push_back({.sent = sent, .ahead = now - sent});
    if (snapshots_.size() > kMostSnapshots) {
      snapshots_.pop_front();
    }
  }
  Snapshot& snapshot = snapshots_.back();
  for (const protocol::Player& player : world.players()) {
    snapshot.players[player.id()] = {player.x(), player.z()};
  }
  for (const protocol::Monster& monster : world.monsters()) {
    snapshot.monsters[monster.id()] = {monster.x(), monster.z()};
  }
}

double Interpolation::Then(double now) const {
  // The world that took least to come shows best how far apart the clocks
  // are: the others were held up on the way by that much more.
  double ahead = std::numeric_limits<double>::infinity();
  for (const Snapshot& snapshot : snapshots_) {
    ahead = std::min(ahead, snapshot.ahead);
  }
  return now - ahead - kDelay;
}

protocol::World Interpolation::Shown(const protocol::World& newest,
                                     double now) const {
  protocol::World shown = newest;
  if (snapshots_.size() < 2) {
    return shown;
  }
  const double then = Then(now);
  // The last snapshot at or before then, and the one after it.
  auto after = std::ranges::find_if(
      snapshots_, [then](const Snapshot& s) { return s.sent > then; });
  if (after == snapshots_.begin() || after == snapshots_.end()) {
    return shown;
  }
  const Snapshot& from = *std::prev(after);
  const Snapshot& to = *after;
  const float share =
      static_cast<float>((then - from.sent) / (to.sent - from.sent));
  Between(*shown.mutable_players(), newest.you(), from.players, to.players,
          share);
  Between(*shown.mutable_monsters(), 0, from.monsters, to.monsters, share);
  return shown;
}

}  // namespace nanommo
