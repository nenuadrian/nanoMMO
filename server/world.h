#ifndef NANOMMO_SERVER_WORLD_H_
#define NANOMMO_SERVER_WORLD_H_

#include <chrono>
#include <cstdint>
#include <map>
#include <random>
#include <string>

#include "terrain/terrain.h"

namespace nanommo {

enum class Motion { kIdle, kWalk, kRun, kAttack, kDead };

struct Player {
  std::string name;
  float x = 0.0f;
  float z = 0.0f;
  /// In radians, around the vertical. Zero faces along z, towards the camera.
  float yaw = 0.0f;
  Motion motion = Motion::kIdle;
  uint32_t experience = 0;
  int health = 0;
  /// Which of the world's lands, by its place in kLands.
  int land = 0;
  uint32_t target = 0;
  std::chrono::nanoseconds swing{0};
  std::chrono::nanoseconds respawn{0};
  float input_x = 0.0f;
  float input_z = 0.0f;
  bool input_run = false;
  bool input_attack = false;
};

struct Monster {
  std::string name;
  float x = 0.0f;
  float z = 0.0f;
  float yaw = 0.0f;
  Motion motion = Motion::kIdle;
  int level = 1;
  int health = 0;
  int max_health = 0;
  uint32_t experience = 0;
  int damage = 0;
  std::chrono::nanoseconds swing_time{0};
  std::chrono::nanoseconds hit_time{0};
  float home_x = 0.0f;
  float home_z = 0.0f;
  float target_x = 0.0f;
  float target_z = 0.0f;
  std::chrono::nanoseconds rest{0};
  uint32_t prey = 0;
  std::chrono::nanoseconds swing{0};
  std::chrono::nanoseconds respawn{0};
};

/// Someone who stands in one place, such as a guard. NPCs don't move or fight,
/// so they have none of a monster's health or of what it thinks with.
struct Npc {
  std::string name;
  float x = 0.0f;
  float z = 0.0f;
  float yaw = 0.0f;
  /// As 0xRRGGBBAA.
  uint32_t color = 0xFFFFFFFF;
  /// In metres. Until NPCs have models, each is a ball this size.
  float radius = 0.5f;
  /// Whether players can talk to it.
  bool talks = false;
};

/// Moves to the place, or if the terrain's in the way there, as far as it can
/// along x or along z alone, so that it slides along a coast.
void Walk(const Terrain& terrain, float& x, float& z, float to_x, float to_z);

class World {
 public:
  /// In tiles a second. A tile is a metre, and that's the walk clip's speed.
  static constexpr float kWalkSpeed = 1.8f;

  static constexpr float kRunSpeed = 3.1f;

  static constexpr float kReach = 1.6f;

  /// As long as the slash animation.
  static constexpr std::chrono::milliseconds kSwingTime{2000};

  static constexpr std::chrono::milliseconds kHitTime{900};

  static constexpr std::chrono::seconds kRespawnTime{10};

  explicit World(uint32_t seed = std::mt19937::default_seed,
                 Terrain terrain = Terrain());

  void Join(uint32_t id, Player player);

  void Leave(uint32_t id);

  void SetInput(uint32_t id, float x, float z, bool run, bool attack = false);

  uint32_t Spawn(Monster monster);

  /// NPCs have ids of their own, apart from the monsters'.
  uint32_t Spawn(Npc npc);

  void Step(std::chrono::nanoseconds time);

  const std::map<uint32_t, Player>& players() const { return players_; }
  const std::map<uint32_t, Monster>& monsters() const { return monsters_; }
  const std::map<uint32_t, Npc>& npcs() const { return npcs_; }
  const Terrain& terrain() const { return terrain_; }

 private:
  void Act(Player& player, std::chrono::nanoseconds time);
  void Move(Player& player, std::chrono::nanoseconds time);
  void Swing(Player& player);
  void Hit(Player& player);
  void Act(Monster& monster, std::chrono::nanoseconds time);
  void Hit(Monster& monster);

  Terrain terrain_;
  std::map<uint32_t, Player> players_;
  std::map<uint32_t, Monster> monsters_;
  uint32_t next_monster_ = 1;
  std::map<uint32_t, Npc> npcs_;
  uint32_t next_npc_ = 1;
  std::mt19937 random_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_WORLD_H_
