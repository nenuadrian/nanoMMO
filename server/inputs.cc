#include "server/inputs.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>

#include "absl/status/status.h"
#include "protocol/clock.h"

namespace nanommo {

namespace {

// How far behind our clock now the input says its client drew everyone else,
// up to the most we believe, or nothing for one that doesn't say, as a bot's.
std::chrono::nanoseconds Behind(const protocol::Input& input) {
  if (input.drawn() == 0) {
    return std::chrono::nanoseconds::zero();
  }
  const int64_t now = NowMicros();
  const int64_t most = std::chrono::microseconds(Inputs::kMostRewind).count();
  const int64_t drawn = std::clamp(input.drawn(), now - most, now);
  return std::chrono::microseconds(now - drawn);
}

}  // namespace

void Inputs::Take(World& land, uint32_t id, const protocol::Input& input) {
  if (input.sequence() != 0) {
    std::deque<protocol::Input>& queued = queued_[id];
    queued.push_back(input);
    if (queued.size() > kMostQueued) {
      queued.pop_front();
    }
  } else if (!queued_.contains(id)) {
    land.SetInput(id, input.x(), input.z(), input.run(), input.attack());
    land.Pick(id, input.target(), input.target_player());
  }
}

void Inputs::Apply(const std::function<World*(uint32_t)>& land_of) {
  for (auto& [id, queued] : queued_) {
    Steady& steady = steady_[id];
    steady.fewest = std::min(steady.fewest, queued.size());
    bool drain = false;
    if (++steady.ticks == kSteadyTicks) {
      drain = steady.fewest >= kMostSteady;
      steady = {};
    }
    World* land = land_of(id);
    if (queued.empty() || land == nullptr) {
      continue;
    }
    // A skipped input counts as applied, and the client corrects the step it
    // walked for it. Draining needs two waiting, so one is left to apply.
    if (queued.size() > kMostBehind || drain) {
      queued.pop_front();
    }
    const protocol::Input& input = queued.front();
    land->SetInput(id, input.x(), input.z(), input.run(), input.attack());
    land->Pick(id, input.target(), input.target_player());
    land->SetBehind(id, Behind(input));
    acked_[id] = input.sequence();
    queued.pop_front();
  }
}

std::optional<uint32_t> Inputs::acked(uint32_t id) const {
  const auto found = acked_.find(id);
  if (found == acked_.end()) {
    return std::nullopt;
  }
  return found->second;
}

void Inputs::Leave(uint32_t id) {
  queued_.erase(id);
  steady_.erase(id);
  acked_.erase(id);
}

absl::Status DoItem(World& land, uint32_t id,
                    const protocol::ItemAction& action) {
  switch (action.action_case()) {
    case protocol::ItemAction::kMove:
      return land.MoveItem(id, action.move().from(), action.move().to(),
                           action.move().count());
    case protocol::ItemAction::kUse:
      return land.UseItem(id, action.use());
    case protocol::ItemAction::kDestroy:
      return land.DestroyItem(id, action.destroy());
    case protocol::ItemAction::ACTION_NOT_SET:
      break;
  }
  return absl::OkStatus();
}

}  // namespace nanommo
