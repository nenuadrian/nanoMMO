#include "server/world.h"

#include <algorithm>
#include <cmath>
#include <iterator>
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

void Walk(const Terrain& terrain, float& x, float& z, float to_x, float to_z) {
  if (terrain.Walkable(to_x, to_z)) {
    x = to_x;
    z = to_z;
  } else if (terrain.Walkable(to_x, z)) {
    x = to_x;
  } else if (terrain.Walkable(x, to_z)) {
    z = to_z;
  }
}

World::World(uint32_t seed, Terrain terrain, ItemTable items, SkillTable skills)
    : terrain_(std::move(terrain)),
      items_(std::move(items)),
      skills_(std::move(skills)),
      random_(seed) {}

void World::Join(uint32_t id, Player player) {
  // Saved somewhere that's sea now, such as on a terrain from another seed.
  if (!terrain_.Walkable(player.x, player.z)) {
    player.x = 0.0f;
    player.z = 0.0f;
  }
  // Zero when none was saved, as for a player from before we kept health.
  if (player.health <= 0) {
    player.health = MaxHealth(LevelFor(player.experience).number);
  }
  // No weapon, or one that isn't in the items file: the starting weapon.
  if (!items_.by_id.contains(player.weapon)) {
    player.weapon = items_.starting_weapon;
  }
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
  monsters_[id] = monster;
  return id;
}

uint32_t World::Spawn(Npc npc) {
  const uint32_t id = next_npc_++;
  npcs_[id] = std::move(npc);
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

void World::UseSkill(uint32_t id, uint32_t skill) {
  const auto found = players_.find(id);
  if (found != players_.end()) {
    found->second.input_skill = skill;
  }
}

void World::Step(std::chrono::nanoseconds time) {
  ZoneScoped;
  for (auto& [id, player] : players_) {
    Act(id, player, time);
  }
  for (auto& [id, monster] : monsters_) {
    Act(monster, time);
  }
}

void World::Act(uint32_t id, Player& player, std::chrono::nanoseconds time) {
  // Cooldowns run down whatever the player's doing.
  for (auto it = player.cooldowns.begin(); it != player.cooldowns.end();) {
    it->second -= time;
    it = it->second <= std::chrono::nanoseconds::zero()
             ? player.cooldowns.erase(it)
             : std::next(it);
  }
  if (player.motion == Motion::kDead) {
    player.input_skill = 0;
    player.respawn -= time;
    if (player.respawn <= std::chrono::nanoseconds::zero()) {
      player.x = 0.0f;
      player.z = 0.0f;
      player.motion = Motion::kIdle;
      player.health = MaxHealth(LevelFor(player.experience).number);
    }
    return;
  }
  if (player.motion == Motion::kSkill) {
    const skills::Skill& skill = skills_.by_id.at(player.skill);
    const auto before = player.cast;
    player.cast += time;
    const std::chrono::milliseconds hit(skill.effect().hit_ms());
    if (before < hit && player.cast >= hit) {
      Land(id, player, skill);
    }
    if (player.cast < std::chrono::milliseconds(skill.effect().cast_ms())) {
      return;
    }
    player.motion = Motion::kIdle;
  }
  const items::Weapon* weapon = WeaponOf(player);
  if (player.motion == Motion::kAttack) {
    const auto before = player.swing;
    player.swing += time;
    const std::chrono::milliseconds hit(weapon->hit_ms());
    if (before < hit && player.swing >= hit) {
      Hit(id, player, *weapon);
    }
    if (player.swing < std::chrono::milliseconds(weapon->swing_ms())) {
      return;
    }
    player.motion = Motion::kIdle;
  }
  // A skill asked for while the player was busy waits until now, and one they
  // can't use yet goes.
  if (const skills::Skill* skill =
          Usable(player, std::exchange(player.input_skill, 0))) {
    Start(player, *skill);
  } else if (player.input_attack && weapon != nullptr) {
    Swing(player, *weapon);
  } else {
    Move(player, time);
  }
}

void World::Move(Player& player, std::chrono::nanoseconds time) {
  const float seconds = std::chrono::duration<float>(time).count();
  const float speed = player.input_run ? kRunSpeed : kWalkSpeed;
  const float edge = terrain_.edge();
  float x = player.x;
  float z = player.z;
  Walk(terrain_, x, z,
       std::clamp(player.x + player.input_x * speed * seconds, -edge, edge),
       std::clamp(player.z + player.input_z * speed * seconds, -edge, edge));
  // Motion comes from how far it actually went, so that a player pushing
  // against the edge or the sea stands still, instead of walking on the spot.
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

const items::Weapon* World::WeaponOf(const Player& player) const {
  const auto found = items_.by_id.find(player.weapon);
  if (found == items_.by_id.end() || !found->second.has_weapon()) {
    return nullptr;
  }
  return &found->second.weapon();
}

// Picks the nearest monster in reach, and turns the player towards it.
void World::Aim(Player& player, float reach) {
  player.target = 0;
  float nearest = reach;
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

void World::Swing(Player& player, const items::Weapon& weapon) {
  player.motion = Motion::kAttack;
  player.swing = std::chrono::nanoseconds::zero();
  Aim(player, weapon.reach());
}

void World::Hit(uint32_t id, Player& player, const items::Weapon& weapon) {
  const auto found = monsters_.find(player.target);
  if (found == monsters_.end()) {
    return;
  }
  Monster& monster = found->second;
  if (monster.motion == Motion::kDead ||
      Distance(player, monster) > weapon.reach()) {
    return;
  }
  const int level = LevelFor(player.experience).number;
  Hurt(id, player, monster,
       static_cast<int>(weapon.damage()) + ExtraDamage(level));
}

const skills::Skill* World::Usable(const Player& player, uint32_t id) const {
  const auto found = skills_.by_id.find(id);
  if (found == skills_.by_id.end() || player.cooldowns.contains(id) ||
      LevelFor(player.experience).number <
          static_cast<int>(found->second.level())) {
    return nullptr;
  }
  return &found->second;
}

void World::Start(Player& player, const skills::Skill& skill) {
  player.motion = Motion::kSkill;
  player.skill = skill.id();
  player.cast = std::chrono::nanoseconds::zero();
  player.cooldowns[skill.id()] = std::chrono::milliseconds(skill.cooldown_ms());
  Aim(player, skill.effect().reach());
}

void World::Land(uint32_t id, Player& player, const skills::Skill& skill) {
  const skills::Effect& effect = skill.effect();
  const int level = LevelFor(player.experience).number;
  player.health = std::min(player.health + static_cast<int>(effect.heal()),
                           MaxHealth(level));
  if (effect.damage() == 0) {
    return;
  }
  const int damage = static_cast<int>(effect.damage()) + ExtraDamage(level);
  for (auto& [monster_id, monster] : monsters_) {
    if ((effect.all() || monster_id == player.target) &&
        monster.motion != Motion::kDead &&
        Distance(player, monster) <= effect.reach()) {
      Push(player, monster, effect.push());
      Hurt(id, player, monster, damage);
    }
  }
}

// Away from the player, like a walk: where the sea or a tree is in the way, it
// slides along it, or stays put.
void World::Push(const Player& player, Monster& monster, float metres) {
  const float distance = Distance(player, monster);
  if (metres == 0.0f || distance == 0.0f) {
    return;
  }
  const float dx = (monster.x - player.x) / distance * metres;
  const float dz = (monster.z - player.z) / distance * metres;
  const float edge = terrain_.edge();
  Walk(terrain_, monster.x, monster.z, std::clamp(monster.x + dx, -edge, edge),
       std::clamp(monster.z + dz, -edge, edge));
}

void World::Hurt(uint32_t id, Player& player, Monster& monster, int damage) {
  const int level = LevelFor(player.experience).number;
  monster.attacker = id;
  monster.health = std::max(monster.health - damage, 0);
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

void World::Act(Monster& monster, std::chrono::nanoseconds time) {
  if (monster.motion == Motion::kDead) {
    Revive(monster, time, random_);
    return;
  }
  if (monster.motion == Motion::kAttack) {
    const auto before = monster.swing;
    monster.swing += time;
    if (before < monster.hit_time && monster.swing >= monster.hit_time) {
      Hit(monster);
    }
    if (monster.swing < monster.swing_time) {
      return;
    }
    monster.motion = Motion::kIdle;
  }
  Hunt(monster, players_, terrain_, time, random_);
}

void World::Hit(Monster& monster) {
  const auto found = players_.find(monster.prey);
  if (found == players_.end()) {
    return;
  }
  Player& player = found->second;
  if (player.motion == Motion::kDead ||
      Distance(player, monster) > monster.reach) {
    return;
  }
  player.health = std::max(player.health - monster.damage, 0);
  if (player.health > 0) {
    return;
  }
  player.motion = Motion::kDead;
  player.respawn = kRespawnTime;
}

}  // namespace nanommo
