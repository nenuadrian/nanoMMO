#include "server/world.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "server/levels.h"
#include "server/monsters.h"
#include "tracy/Tracy.hpp"

namespace nanommo {

namespace {

float Distance(const Player& player, const Monster& monster) {
  return std::hypot(monster.x - player.x, monster.z - player.z);
}

}  // namespace

World::World(uint32_t seed) : random_(seed) {}

void World::Join(uint32_t id, Player player) {
  player.health = MaxHealth(LevelFor(player.experience).number);
  players_[id] = std::move(player);
}

void World::Leave(uint32_t id) { players_.erase(id); }

uint32_t World::Spawn(Monster monster) {
  monster.home_x = monster.x;
  monster.home_z = monster.z;
  monster.health = monster.max_health;
  // A rest of its own, so that the monsters don't all set off together.
  monster.rest = RandomRest(random_);
  const uint32_t id = next_monster_++;
  monsters_[id] = std::move(monster);
  return id;
}

void World::SetInput(uint32_t id, float x, float z, bool run, bool attack) {
  const auto found = players_.find(id);
  if (found == players_.end() || !std::isfinite(x) || !std::isfinite(z)) {
    return;
  }
  // So that moving diagonally isn't faster, and a modified client can't ask
  // for more speed.
  const float length = std::hypot(x, z);
  if (length > 1.0f) {
    x /= length;
    z /= length;
  }
  found->second.input_x = x;
  found->second.input_z = z;
  found->second.input_run = run;
  found->second.input_attack = attack;
}

void World::Step(std::chrono::nanoseconds time) {
  ZoneScoped;
  for (auto& [id, player] : players_) {
    Act(player, time);
  }
  for (auto& [id, monster] : monsters_) {
    if (monster.motion == Motion::kDead) {
      Revive(monster, time, random_);
    } else {
      Wander(monster, time, random_);
    }
  }
}

void World::Act(Player& player, std::chrono::nanoseconds time) {
  if (player.motion == Motion::kAttack) {
    const auto before = player.swing;
    player.swing += time;
    if (before < kHitTime && player.swing >= kHitTime) {
      Hit(player);
    }
    if (player.swing < kSwingTime) {
      return;
    }
    player.motion = Motion::kIdle;
  }
  if (player.input_attack) {
    Swing(player);
  } else {
    Move(player, time);
  }
}

void World::Move(Player& player, std::chrono::nanoseconds time) {
  const float seconds = std::chrono::duration<float>(time).count();
  const float speed = player.input_run ? kRunSpeed : kWalkSpeed;
  const float x =
      std::clamp(player.x + player.input_x * speed * seconds, -kEdge, kEdge);
  const float z =
      std::clamp(player.z + player.input_z * speed * seconds, -kEdge, kEdge);
  // Motion comes from how far it actually went, so that a player pushing
  // against the edge stands still, instead of walking on the spot.
  const float dx = x - player.x;
  const float dz = z - player.z;
  if (dx == 0.0f && dz == 0.0f) {
    player.motion = Motion::kIdle;
  } else {
    player.motion = player.input_run ? Motion::kRun : Motion::kWalk;
    player.yaw = std::atan2(dx, dz);
  }
  player.x = x;
  player.z = z;
}

void World::Swing(Player& player) {
  player.motion = Motion::kAttack;
  player.swing = std::chrono::nanoseconds::zero();
  player.target = 0;
  float nearest = kReach;
  for (const auto& [id, monster] : monsters_) {
    if (monster.motion != Motion::kDead &&
        Distance(player, monster) <= nearest) {
      player.target = id;
      nearest = Distance(player, monster);
    }
  }
  if (player.target != 0) {
    const Monster& monster = monsters_.at(player.target);
    player.yaw = std::atan2(monster.x - player.x, monster.z - player.z);
  }
}

void World::Hit(Player& player) {
  const auto found = monsters_.find(player.target);
  if (found == monsters_.end()) {
    return;
  }
  Monster& monster = found->second;
  if (monster.motion == Motion::kDead || Distance(player, monster) > kReach) {
    return;
  }
  const int level = LevelFor(player.experience).number;
  monster.health = std::max(monster.health - Damage(level), 0);
  if (monster.health > 0) {
    return;
  }
  monster.motion = Motion::kDead;
  monster.respawn = kRespawnTime;
  player.experience += monster.experience;
  const int now = LevelFor(player.experience).number;
  if (now > level) {
    player.health = MaxHealth(now);
  }
}

}  // namespace nanommo
