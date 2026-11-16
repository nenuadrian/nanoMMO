#include "server/duels.h"

#include <chrono>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

namespace nanommo {

void Duels::Ask(uint32_t id, uint32_t other) {
  duels_[id] = {.challenger = id, .challenged = other};
  challengers_[id] = id;
  challengers_[other] = id;
  news_.insert({id, other});
}

uint32_t Duels::Accept(uint32_t id, Place flag) {
  const auto in = challengers_.find(id);
  if (in == challengers_.end() || in->second == id) {
    return 0;
  }
  Duel& duel = duels_.at(in->second);
  if (duel.state != State::kAsked) {
    return 0;
  }
  duel.state = State::kCountdown;
  duel.left = kCountdown;
  duel.flag = flag;
  news_.insert({id, duel.challenger});
  return duel.challenger;
}

const Duels::Duel* Duels::Of(uint32_t id) const {
  const auto in = challengers_.find(id);
  return in != challengers_.end() ? &duels_.at(in->second) : nullptr;
}

uint32_t Duels::Opponent(uint32_t id) const {
  const Duel* duel = Of(id);
  return duel != nullptr && duel->state == State::kFighting ? duel->Against(id)
                                                            : 0;
}

uint32_t Duels::End(uint32_t id) {
  const auto in = challengers_.find(id);
  if (in == challengers_.end()) {
    return 0;
  }
  const auto duel = duels_.find(in->second);
  const uint32_t other = duel->second.Against(id);
  duels_.erase(duel);
  challengers_.erase(id);
  challengers_.erase(other);
  news_.insert({id, other});
  return other;
}

std::vector<Duels::Duel> Duels::Step(std::chrono::nanoseconds time) {
  std::vector<Duel> ran_out;
  for (auto& [challenger, duel] : duels_) {
    if (duel.state == State::kFighting) {
      continue;
    }
    const std::chrono::nanoseconds before = duel.left;
    duel.left -= time;
    if (duel.state == State::kAsked) {
      if (duel.left <= std::chrono::nanoseconds::zero()) {
        ran_out.push_back(duel);
      }
      continue;
    }
    // Both sides hear as each second of the countdown goes, so that each
    // shows the seconds the server has left, and as it ends.
    if (duel.left <= std::chrono::nanoseconds::zero()) {
      duel.state = State::kFighting;
      duel.left = std::chrono::nanoseconds::zero();
      news_.insert({duel.challenger, duel.challenged});
    } else if (std::chrono::ceil<std::chrono::seconds>(duel.left) <
               std::chrono::ceil<std::chrono::seconds>(before)) {
      news_.insert({duel.challenger, duel.challenged});
    }
  }
  for (const Duel& duel : ran_out) {
    End(duel.challenger);
  }
  return ran_out;
}

std::set<uint32_t> Duels::TakeNews() { return std::exchange(news_, {}); }

}  // namespace nanommo
