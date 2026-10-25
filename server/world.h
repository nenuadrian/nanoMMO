#ifndef NANOMMO_SERVER_WORLD_H_
#define NANOMMO_SERVER_WORLD_H_

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>

#include "absl/status/status.h"
#include "content/content.h"
#include "protocol/nanommo.pb.h"
#include "server/inventory.h"
#include "terrain/terrain.h"

namespace nanommo {

enum class Motion { kIdle, kWalk, kRun, kAttack, kDead, kSkill };

/// What a player's worn items add up to, worked out as what they wear
/// changes, so that a fight doesn't add it up for each blow.
struct Stats {
  int damage = 0;
  int health = 0;
  int armour = 0;
  /// The weapon in the player's main hand, from the content, or nullptr.
  const items::Weapon* weapon = nullptr;
};

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
  /// What the player carries and wears.
  Inventory inventory;
  Stats stats;
  /// The skills the player knows, by their ids in the skills file.
  std::set<uint32_t> skills;
  uint32_t target = 0;
  std::chrono::nanoseconds swing{0};
  /// The skill the player's using, by its id in the skills file, and how far
  /// into it they are.
  uint32_t skill = 0;
  std::chrono::nanoseconds cast{0};
  /// How long until each skill they've used is ready again, by its id.
  std::map<uint32_t, std::chrono::nanoseconds> cooldowns;
  std::chrono::nanoseconds respawn{0};
  float input_x = 0.0f;
  float input_z = 0.0f;
  bool input_run = false;
  bool input_attack = false;
  /// A skill the player asked for, which waits until they're free.
  uint32_t input_skill = 0;
};

struct Monster {
  /// By its id in the monsters file.
  uint32_t kind = 0;
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
  /// In metres, from its kind's fight.
  float reach = 0.0f;
  float sight = 0.0f;
  float leash = 0.0f;
  float wander = 0.0f;
  float home_x = 0.0f;
  float home_z = 0.0f;
  float target_x = 0.0f;
  float target_z = 0.0f;
  std::chrono::nanoseconds rest{0};
  uint32_t prey = 0;
  /// The player who hurt it last, whom it goes for, seen or not, while they're
  /// within its leash.
  uint32_t attacker = 0;
  std::chrono::nanoseconds swing{0};
  std::chrono::nanoseconds respawn{0};
};

/// Someone who stands in one place, such as a guard. NPCs don't move or fight,
/// so they have none of a monster's health or of what it thinks with.
struct Npc {
  std::string name;
  /// By its id in the NPCs file.
  uint32_t kind = 0;
  float x = 0.0f;
  float z = 0.0f;
  float yaw = 0.0f;
  /// Whether players can talk to it.
  bool talks = false;
  /// The skills it teaches, by their ids in the skills file.
  std::set<uint32_t> teaches;
};

/// The most health the player can have: their level's, and what they wear
/// adds.
int MaxHealthOf(const Player& player);

/// What the player holds in their main hand, by its id in the items file, or
/// zero.
uint32_t MainHandOf(const Player& player);

/// Moves to the place, or if the terrain's in the way there, as far as it can
/// along x or along z alone, so that it slides along a coast.
void Walk(const Terrain& terrain, float& x, float& z, float to_x, float to_z);

class World {
 public:
  /// In tiles a second. A tile is a metre, and that's the walk clip's speed.
  static constexpr float kWalkSpeed = 1.8f;

  static constexpr float kRunSpeed = 3.1f;

  static constexpr std::chrono::seconds kRespawnTime{10};

  /// In metres, from the middle of an NPC, as you'd talk to someone.
  static constexpr float kTalkReach = 4.0f;

  /// Players fight with the content's weapons and skills, so in a world
  /// without content, nobody can. Content doesn't change as the game runs, so
  /// the world's copies of its lands share one.
  explicit World(
      uint32_t seed = std::mt19937::default_seed, Terrain terrain = Terrain(),
      std::shared_ptr<const Content> content = std::make_shared<Content>());

  void Join(uint32_t id, Player player);

  void Leave(uint32_t id);

  void SetInput(uint32_t id, float x, float z, bool run, bool attack = false);

  /// The player uses the skill as soon as they're free, if they know it and
  /// it's ready.
  void UseSkill(uint32_t id, uint32_t skill);

  /// The NPC, a trainer in this land, teaches the player the skill, or the
  /// error says why not, in words for the player.
  absl::Status Learn(uint32_t id, uint32_t npc, uint32_t skill);

  /// Moves the player's items around, as Inventory::Move does, wearing them
  /// and taking them off, or the error says why not, in words for the player.
  absl::Status MoveItem(uint32_t id, const protocol::Place& from,
                        const protocol::Place& to, uint32_t count = 0);

  /// Throws away all of what's at the place.
  absl::Status DestroyItem(uint32_t id, const protocol::Place& place);

  uint32_t Spawn(Monster monster);

  /// NPCs have ids of their own, apart from the monsters'.
  uint32_t Spawn(Npc npc);

  void Step(std::chrono::nanoseconds time);

  const std::map<uint32_t, Player>& players() const { return players_; }
  const std::map<uint32_t, Monster>& monsters() const { return monsters_; }
  const std::map<uint32_t, Npc>& npcs() const { return npcs_; }
  const Terrain& terrain() const { return terrain_; }

 private:
  /// After what the player wears has changed: works out their stats again.
  void Dress(Player& player) const;
  void Act(uint32_t id, Player& player, std::chrono::nanoseconds time);
  void Move(Player& player, std::chrono::nanoseconds time);
  void Aim(Player& player, float reach);
  void Swing(Player& player, const items::Weapon& weapon);
  void Hit(uint32_t id, Player& player, const items::Weapon& weapon);
  const skills::Skill* Usable(const Player& player, uint32_t id) const;
  void Start(Player& player, const skills::Skill& skill);
  void Land(uint32_t id, Player& player, const skills::Skill& skill);
  void Push(const Player& player, Monster& monster, float metres);
  void Hurt(uint32_t id, Player& player, Monster& monster, int damage);
  void Act(Monster& monster, std::chrono::nanoseconds time);
  void Hit(Monster& monster);

  Terrain terrain_;
  std::shared_ptr<const Content> content_;
  std::map<uint32_t, Player> players_;
  std::map<uint32_t, Monster> monsters_;
  uint32_t next_monster_ = 1;
  std::map<uint32_t, Npc> npcs_;
  uint32_t next_npc_ = 1;
  std::mt19937 random_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_WORLD_H_
