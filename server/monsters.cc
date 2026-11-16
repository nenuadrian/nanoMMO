#include "server/monsters.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <utility>
#include <vector>

#include "terrain/lands.h"

namespace nanommo {

namespace {

constexpr float kCampsFromTheMiddle = 30.0f;

// So many places tried for each camp, before the land counts as full.
constexpr int kTriesEach = 50;

// Above the beach, and below the rock.
constexpr float kLowestCamp = 1.0f;
constexpr float kHighestCamp = 10.0f;

// Where each camp's three monsters stand, from the middle of the camp.
constexpr std::array<std::pair<float, float>, 3> kCampLayout = {{
    {-1.5f, 0.0f},
    {1.5f, 0.5f},
    {0.0f, -1.5f},
}};

// The square root spreads the places evenly, not bunched up around home.
void PickTarget(Monster& monster, const Terrain& terrain,
                std::mt19937& random) {
  std::uniform_real_distribution<float> unit(0.0f, 1.0f);
  const float angle = 2.0f * std::numbers::pi_v<float> * unit(random);
  const float distance = monster.wander * std::sqrt(unit(random));
  monster.target_x = std::clamp(monster.home_x + distance * std::sin(angle),
                                -terrain.edge(), terrain.edge());
  monster.target_z = std::clamp(monster.home_z + distance * std::cos(angle),
                                -terrain.edge(), terrain.edge());
}

// Whether the monster uses the skill, by its id, now: a boss, only if its
// phase does.
bool Uses(const Monster& monster, uint32_t skill) {
  if (!monster.boss()) {
    return true;
  }
  const auto& skills = monster.phases[monster.phase].skills();
  return std::ranges::find(skills, skill) != skills.end();
}

// Runs a boss home, at the players' running speed. Once it's there, or
// something's in its way, it's home, so that a tree can't keep it from
// starting again, and it rests.
void GoHome(Monster& monster, const Terrain& terrain,
            std::chrono::nanoseconds time, std::mt19937& random) {
  const float dx = monster.home_x - monster.x;
  const float dz = monster.home_z - monster.z;
  const float distance = std::hypot(dx, dz);
  const float step =
      World::kRunSpeed * std::chrono::duration<float>(time).count();
  const float x = monster.x;
  const float z = monster.z;
  if (distance > step) {
    Walk(terrain, monster.x, monster.z, x + dx / distance * step,
         z + dz / distance * step);
    monster.yaw = std::atan2(dx, dz);
    monster.motion = Motion::kRun;
  }
  if (distance <= step || (monster.x == x && monster.z == z)) {
    monster.x = monster.home_x;
    monster.z = monster.home_z;
    monster.evading = false;
    monster.motion = Motion::kIdle;
    monster.rest = RandomRest(random);
  }
}

}  // namespace

Monster MonsterOf(const monsters::Kind& kind, float x, float z) {
  const monsters::Fight& fight = kind.fight();
  return {.kind = kind.id(),
          .x = x,
          .z = z,
          .level = static_cast<int>(kind.level()),
          .max_health = static_cast<int>(kind.max_health()),
          .experience = kind.experience(),
          .damage = static_cast<int>(fight.damage()),
          .swing_time = std::chrono::milliseconds(fight.swing_ms()),
          .hit_time = std::chrono::milliseconds(fight.hit_ms()),
          .reach = fight.reach(),
          .sight = fight.sight(),
          .leash = fight.leash(),
          .wander = fight.wander(),
          .skills = {kind.skills().begin(), kind.skills().end()},
          .phases = {kind.phases().begin(), kind.phases().end()}};
}

void SpawnMonsters(World& land, int index, const MonsterTable& table) {
  for (const monsters::Spawn& spawn : table.spawns) {
    if (LandNamed(spawn.land()) == index) {
      land.Spawn(MonsterOf(table.kinds.at(spawn.kind()), spawn.x(), spawn.z()));
    }
  }
  const Terrain& terrain = land.terrain();
  for (const monsters::Camps& camps : table.camps) {
    if (LandNamed(camps.land()) != index) {
      continue;
    }
    const monsters::Kind& kind = table.kinds.at(camps.kind());
    for (const auto& [camp_x, camp_z] :
         Camps(terrain, static_cast<int>(camps.count()))) {
      for (const auto& [x, z] : kCampLayout) {
        if (terrain.Walkable(camp_x + x, camp_z + z)) {
          land.Spawn(MonsterOf(kind, camp_x + x, camp_z + z));
        }
      }
    }
  }
}

std::vector<std::pair<float, float>> Camps(const Terrain& terrain, int count) {
  std::mt19937 random(terrain.seed());
  std::uniform_real_distribution<float> place(-terrain.edge(), terrain.edge());
  std::vector<std::pair<float, float>> camps;
  // Darts thrown at the land: a place too near a camp we have is thrown
  // again, as is one in the sea, so the camps spread over the land evenly,
  // where places picked at random alone bunch up and leave gaps.
  const auto spaced = [&camps](float x, float z) {
    return std::ranges::none_of(camps, [x, z](const auto& camp) {
      return std::hypot(camp.first - x, camp.second - z) < kCampSpacing;
    });
  };
  for (int tries = 0; tries < kTriesEach * count && std::ssize(camps) < count;
       ++tries) {
    const float x = place(random);
    const float z = place(random);
    const float height = terrain.Height(x, z);
    if (std::hypot(x, z) >= kCampsFromTheMiddle && height >= kLowestCamp &&
        height <= kHighestCamp && terrain.Walkable(x, z) && spaced(x, z)) {
      camps.emplace_back(x, z);
    }
  }
  return camps;
}

std::chrono::nanoseconds RandomRest(std::mt19937& random) {
  std::uniform_int_distribution<int> ms(kShortestRest.count(),
                                        kLongestRest.count());
  return std::chrono::milliseconds(ms(random));
}

void Wander(Monster& monster, const Terrain& terrain,
            std::chrono::nanoseconds time, std::mt19937& random) {
  if (monster.motion == Motion::kIdle) {
    monster.rest -= time;
    if (monster.rest > std::chrono::nanoseconds::zero()) {
      return;
    }
    PickTarget(monster, terrain, random);
    monster.motion = Motion::kWalk;
  }

  const float dx = monster.target_x - monster.x;
  const float dz = monster.target_z - monster.z;
  const float distance = std::hypot(dx, dz);
  const float step =
      World::kWalkSpeed * std::chrono::duration<float>(time).count();
  const float x = monster.x;
  const float z = monster.z;
  if (distance <= step) {
    Walk(terrain, monster.x, monster.z, monster.target_x, monster.target_z);
  } else {
    Walk(terrain, monster.x, monster.z, x + dx / distance * step,
         z + dz / distance * step);
    monster.yaw = std::atan2(dx, dz);
  }
  // There, or stuck at the sea, it rests before it picks another place.
  if (distance <= step || (monster.x == x && monster.z == z)) {
    monster.motion = Motion::kIdle;
    monster.rest = RandomRest(random);
  }
}

uint32_t Spot(const Monster& monster,
              const std::map<uint32_t, Player>& players) {
  const auto in_leash = [&monster](const Player& player) {
    return player.motion != Motion::kDead &&
           std::hypot(player.x - monster.home_x, player.z - monster.home_z) <=
               monster.leash;
  };
  // Seen or not, so that a kick that knocks it out of sight doesn't make it
  // forget them. On a tie, it keeps to the one it's fighting.
  uint32_t hated = 0;
  int most = 0;
  for (const auto& [id, threat] : monster.threat) {
    const auto player = players.find(id);
    if (player != players.end() && in_leash(player->second) &&
        (threat > most || (threat == most && id == monster.prey))) {
      hated = id;
      most = threat;
    }
  }
  if (hated != 0) {
    return hated;
  }
  uint32_t nearest = 0;
  float closest = monster.sight;
  for (const auto& [id, player] : players) {
    const float away = std::hypot(player.x - monster.x, player.z - monster.z);
    if (in_leash(player) && away <= closest) {
      nearest = id;
      closest = away;
    }
  }
  return nearest;
}

void Hunt(Monster& monster, const std::map<uint32_t, Player>& players,
          const Terrain& terrain, std::chrono::nanoseconds time,
          std::mt19937& random) {
  if (monster.evading) {
    GoHome(monster, terrain, time, random);
    return;
  }
  monster.prey = Spot(monster, players);
  // Once everyone who's hurt it has died, left or got past its leash, it
  // forgets them. A boss goes home to start again, healed, so that a fight
  // that's lost doesn't leave it weaker for the next.
  if (!monster.threat.empty() && !monster.threat.contains(monster.prey)) {
    monster.threat.clear();
    if (monster.boss()) {
      monster.evading = true;
      monster.health = monster.max_health;
      monster.phase = 0;
      monster.prey = 0;
      GoHome(monster, terrain, time, random);
      return;
    }
  }
  if (monster.prey == 0) {
    if (monster.motion == Motion::kRun) {
      monster.motion = Motion::kIdle;
      monster.rest = RandomRest(random);
    }
    Wander(monster, terrain, time, random);
    return;
  }

  const Player& player = players.at(monster.prey);
  const float dx = player.x - monster.x;
  const float dz = player.z - monster.z;
  const float distance = std::hypot(dx, dz);
  monster.yaw = std::atan2(dx, dz);
  // A skill that's ready goes in place of a swing: the monster stands where
  // it is, facing the player, while the skill's shape shows. A boss uses its
  // phase's.
  for (const monsters::Skill& skill : monster.skills) {
    if (Uses(monster, skill.id()) && distance <= skill.range() &&
        !monster.cooldowns.contains(skill.id())) {
      monster.motion = Motion::kSkill;
      monster.casting = skill;
      monster.cast = std::chrono::nanoseconds::zero();
      monster.cast_x = monster.x;
      monster.cast_z = monster.z;
      monster.cooldowns[skill.id()] =
          std::chrono::milliseconds(skill.cooldown_ms());
      return;
    }
  }
  if (distance <= monster.reach) {
    monster.motion = Motion::kAttack;
    monster.swing = std::chrono::nanoseconds::zero();
    return;
  }
  const float step =
      std::min(World::kRunSpeed * std::chrono::duration<float>(time).count(),
               distance - monster.reach);
  Walk(terrain, monster.x, monster.z, monster.x + dx / distance * step,
       monster.z + dz / distance * step);
  monster.motion = Motion::kRun;
}

void Revive(Monster& monster, std::chrono::nanoseconds time,
            std::mt19937& random) {
  monster.respawn -= time;
  if (monster.respawn > std::chrono::nanoseconds::zero()) {
    return;
  }
  monster.x = monster.home_x;
  monster.z = monster.home_z;
  monster.health = monster.max_health;
  monster.phase = 0;
  monster.motion = Motion::kIdle;
  monster.threat.clear();
  monster.rest = RandomRest(random);
}

}  // namespace nanommo
