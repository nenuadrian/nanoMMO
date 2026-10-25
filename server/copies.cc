#include "server/copies.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <map>
#include <random>
#include <utility>
#include <vector>

#include "server/monsters.h"
#include "server/npcs.h"
#include "terrain/lands.h"

namespace nanommo {

Copies::Copies(uint32_t seed, Content content)
    : content_(std::make_shared<const Content>(std::move(content))) {
  for (const Land& land : kLands) {
    terrains_.emplace_back(seed + land.seed, land.wading);
  }
  for (int land = 0; land < std::ssize(kLands); ++land) {
    if (!kLands[land].dungeon) {
      For(land, 0);
    }
  }
}

void Copies::Join(uint32_t id, Player player) {
  const uint32_t copy = For(player.land, id);
  copies_.at(copy).world.Join(id, std::move(player));
  where_[id] = copy;
}

void Copies::Leave(uint32_t id) {
  const auto where = where_.find(id);
  if (where == where_.end()) {
    return;
  }
  copies_.at(where->second).world.Leave(id);
  where_.erase(where);
}

Copy* Copies::Of(uint32_t id) {
  const auto where = where_.find(id);
  return where != where_.end() ? &copies_.at(where->second) : nullptr;
}

int Copies::players() const {
  int count = 0;
  for (const auto& [id, copy] : copies_) {
    count += static_cast<int>(copy.world.players().size());
  }
  return count;
}

void Copies::Step(std::chrono::nanoseconds time) {
  for (auto it = copies_.begin(); it != copies_.end();) {
    Copy& copy = it->second;
    if (!kLands[copy.land].dungeon || !copy.world.players().empty()) {
      copy.empty = std::chrono::nanoseconds::zero();
      copy.world.Step(time);
      ++it;
      continue;
    }
    copy.empty += time;
    if (copy.empty < kEmptyLife) {
      ++it;
      continue;
    }
    std::cout << "dropped player " << copy.owner << "'s copy of the "
              << kLands[copy.land].name << std::endl;
    it = copies_.erase(it);
  }
  Travel();
}

uint32_t Copies::For(int land, uint32_t id) {
  const uint32_t owner = kLands[land].dungeon ? id : 0;
  for (const auto& [copy_id, copy] : copies_) {
    if (copy.land == land && copy.owner == owner) {
      return copy_id;
    }
  }
  const uint32_t made = next_++;
  Copy& copy =
      copies_
          .emplace(made, Copy{.land = land,
                              .world = World(std::random_device()(),
                                             terrains_[land], content_),
                              .owner = owner})
          .first->second;
  SpawnMonsters(copy.world, land, content_->monsters);
  SpawnNpcs(copy.world, land, content_->npcs);
  copy.npc_cells = CellsOf(copy.world.npcs());
  if (owner != 0) {
    std::cout << "made player " << owner << " a copy of the "
              << kLands[land].name << std::endl;
  }
  return made;
}

void Copies::Travel() {
  // A new copy goes into the map as we go through it, which std::map allows,
  // and the players who arrive in it are out of its gate.
  for (auto& [from, copy] : copies_) {
    // Leaving changes the map we go through, so first we note who's going,
    // and through which gate.
    std::vector<std::pair<uint32_t, const Gate*>> travellers;
    for (const auto& [id, player] : copy.world.players()) {
      for (const Gate& gate : kGates) {
        if (gate.land == copy.land && InGate(gate, player.x, player.z)) {
          travellers.emplace_back(id, &gate);
          break;
        }
      }
    }
    for (const auto& [id, gate] : travellers) {
      Player player = copy.world.players().at(id);
      copy.world.Leave(id);
      const Place arrival = Arrival(*gate, player.yaw);
      player.x = arrival.x;
      player.z = arrival.z;
      player.land = gate->to;
      std::cout << player.name << " went to the " << kLands[gate->to].name
                << std::endl;
      const uint32_t to = For(gate->to, id);
      copies_.at(to).world.Join(id, std::move(player));
      where_[id] = to;
    }
  }
}

}  // namespace nanommo
