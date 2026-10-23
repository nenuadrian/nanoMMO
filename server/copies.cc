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
#include "terrain/terrain.h"

namespace nanommo {

Copies::Copies(uint32_t seed, const Content& content) {
  for (int land = 0; land < std::ssize(kLands); ++land) {
    Copy& copy =
        copies_
            .emplace(next_++,
                     Copy{.land = land,
                          .world = World(std::random_device()(),
                                         Terrain(seed + kLands[land].seed,
                                                 kLands[land].wading),
                                         content.items, content.skills)})
            .first->second;
    SpawnMonsters(copy.world, land, content.monsters);
    SpawnNpcs(copy.world, land, content.npcs);
    copy.npc_cells = CellsOf(copy.world.npcs());
  }
}

void Copies::Join(uint32_t id, Player player) {
  const uint32_t copy = For(player.land);
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
  for (auto& [id, copy] : copies_) {
    copy.world.Step(time);
  }
  Travel();
}

uint32_t Copies::For(int land) const {
  // Each land has a copy from the start.
  for (const auto& [copy_id, copy] : copies_) {
    if (copy.land == land) {
      return copy_id;
    }
  }
  return 0;
}

void Copies::Travel() {
  for (auto& [from, copy] : copies_) {
    const Land& here = kLands[copy.land];
    // Leaving changes the map we go through, so first we note who's going.
    std::vector<uint32_t> travellers;
    for (const auto& [id, player] : copy.world.players()) {
      if (InGate(here, player.x, player.z)) {
        travellers.push_back(id);
      }
    }
    for (const uint32_t id : travellers) {
      Player player = copy.world.players().at(id);
      copy.world.Leave(id);
      const Land& there = kLands[here.to];
      const Place arrival = Arrival(there, player.yaw);
      player.x = arrival.x;
      player.z = arrival.z;
      player.land = here.to;
      std::cout << player.name << " went to the " << there.name << std::endl;
      const uint32_t to = For(here.to);
      copies_.at(to).world.Join(id, std::move(player));
      where_[id] = to;
    }
  }
}

}  // namespace nanommo
