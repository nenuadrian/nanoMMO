#include "server/world.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "server/levels.h"
#include "server/monsters.h"
#include "tracy/Tracy.hpp"

namespace nanommo {

namespace {

float Distance(const Player& player, const Monster& monster) {
  return std::hypot(monster.x - player.x, monster.z - player.z);
}

void RunDown(std::map<uint32_t, std::chrono::nanoseconds>& cooldowns,
             std::chrono::nanoseconds time) {
  for (auto it = cooldowns.begin(); it != cooldowns.end();) {
    it->second -= time;
    it = it->second <= std::chrono::nanoseconds::zero() ? cooldowns.erase(it)
                                                        : std::next(it);
  }
}

void Add(Stats& stats, const items::Modifier& modifier) {
  switch (modifier.stat()) {
    case items::STAT_DAMAGE:
      stats.damage += modifier.amount();
      break;
    case items::STAT_HEALTH:
      stats.health += modifier.amount();
      break;
    case items::STAT_ARMOUR:
      stats.armour += modifier.amount();
      break;
    default:
      break;
  }
}

// "a Bone", "an Ash Blade", or "3 Bones".
std::string Some(const items::Item& item, uint32_t count) {
  if (count > 1) {
    return absl::StrCat(count, " ", item.name(), "s");
  }
  const bool vowel = !item.name().empty() &&
                     std::string_view("AEIOU").find(item.name().front()) !=
                         std::string_view::npos;
  return absl::StrCat(vowel ? "an " : "a ", item.name());
}

// "A", "A and B", or "A, B and C".
std::string List(const std::vector<std::string>& things) {
  std::string list;
  for (std::size_t i = 0; i < things.size(); ++i) {
    absl::StrAppend(&list,
                    i == 0                   ? ""
                    : i + 1 == things.size() ? " and "
                                             : ", ",
                    things[i]);
  }
  return list;
}

}  // namespace

int MaxHealthOf(const Player& player) {
  return MaxHealth(LevelFor(player.experience).number) + player.stats.health;
}

namespace {

// Adds to the player's experience, and heals them fully as they reach a new
// level.
void Gain(Player& player, uint32_t experience) {
  const int level = LevelFor(player.experience).number;
  player.experience += experience;
  if (LevelFor(player.experience).number > level) {
    player.health = MaxHealthOf(player);
  }
}

}  // namespace

uint32_t MainHandOf(const Player& player) {
  const items::Stack* held = player.inventory.At(Worn(items::SLOT_MAIN_HAND));
  return held != nullptr ? held->item() : 0;
}

std::vector<uint32_t> WornOf(const Player& player) {
  std::vector<uint32_t> worn;
  for (const items::Slot slot :
       {items::SLOT_OFF_HAND, items::SLOT_HEAD, items::SLOT_BODY,
        items::SLOT_LEGS, items::SLOT_FEET}) {
    if (const items::Stack* item = player.inventory.At(Worn(slot))) {
      worn.push_back(item->item());
    }
  }
  return worn;
}

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

World::World(uint32_t seed, Terrain terrain,
             std::shared_ptr<const Content> content)
    : terrain_(std::move(terrain)),
      content_(std::move(content)),
      random_(seed) {}

void World::Join(uint32_t id, Player player) {
  // Saved somewhere that's sea now, such as on a terrain from another seed.
  if (!terrain_.Walkable(player.x, player.z)) {
    player.x = 0.0f;
    player.z = 0.0f;
  }
  // A bag that's never had a revision has never been anyone's: a new player,
  // or one from before bags, gets the kit, once, which a trip through a gate
  // doesn't give them again.
  const ItemTable& items = content_->items;
  if (player.inventory.revision() == 0) {
    for (const items::Stack& stack : items.starting) {
      if (!player.inventory.Wear(items, stack)) {
        player.inventory.Add(items, stack);
      }
    }
  }
  Dress(player);
  // Zero when none was saved, as for a player from before we kept health.
  if (player.health <= 0) {
    player.health = MaxHealthOf(player);
  }
  player.skills.insert(content_->skills.starting.begin(),
                       content_->skills.starting.end());
  players_[id] = std::move(player);
}

void World::Leave(uint32_t id) { players_.erase(id); }

absl::Status World::MoveItem(uint32_t id, const protocol::Place& from,
                             const protocol::Place& to, uint32_t count) {
  const auto found = players_.find(id);
  if (found == players_.end()) {
    return absl::NotFoundError("There's nobody to move it.");
  }
  Player& player = found->second;
  const absl::Status moved =
      player.inventory.Move(content_->items, from, to, count);
  if (moved.ok() && (from.has_worn() || to.has_worn())) {
    Dress(player);
  }
  return moved;
}

absl::Status World::DestroyItem(uint32_t id, const protocol::Place& place) {
  const auto found = players_.find(id);
  if (found == players_.end()) {
    return absl::NotFoundError("There's nobody to throw it away.");
  }
  Player& player = found->second;
  const absl::Status destroyed = player.inventory.Destroy(place);
  if (destroyed.ok() && place.has_worn()) {
    Dress(player);
  }
  return destroyed;
}

absl::Status World::UseItem(uint32_t id, uint32_t slot) {
  const auto found = players_.find(id);
  if (found == players_.end()) {
    return absl::NotFoundError("There's nobody to use it.");
  }
  Player& player = found->second;
  const protocol::Place place = InBag(slot);
  const items::Stack* stack = player.inventory.At(place);
  if (stack == nullptr) {
    return absl::NotFoundError("There's nothing there.");
  }
  const auto item = content_->items.by_id.find(stack->item());
  if (item == content_->items.by_id.end() || !item->second.has_use()) {
    return absl::FailedPreconditionError(absl::StrCat(
        "You can't use ",
        item != content_->items.by_id.end() ? "the " + item->second.name()
                                            : "that",
        "."));
  }
  const std::string& name = item->second.name();
  const items::Use& use = item->second.use();
  if (player.motion == Motion::kDead) {
    return absl::FailedPreconditionError("You can't, while you're dead.");
  }
  const auto cooling = player.item_cooldowns.find(item->first);
  if (cooling != player.item_cooldowns.end()) {
    return absl::FailedPreconditionError(absl::StrCat(
        "You can use another ", name, " in ",
        std::chrono::ceil<std::chrono::seconds>(cooling->second).count(),
        " s."));
  }
  if (use.heal() > 0 && player.health >= MaxHealthOf(player)) {
    return absl::FailedPreconditionError("You're at full health already.");
  }
  player.health = std::min(player.health + static_cast<int>(use.heal()),
                           MaxHealthOf(player));
  if (use.cooldown_ms() > 0) {
    player.item_cooldowns[item->first] =
        std::chrono::milliseconds(use.cooldown_ms());
  }
  return player.inventory.TakeOne(place);
}

void World::Dress(Player& player) const {
  const items::Weapon* before = player.stats.weapon;
  player.stats = {};
  for (const auto& [slot, stack] : player.inventory.items().worn()) {
    const auto item = content_->items.by_id.find(stack.item());
    if (item == content_->items.by_id.end()) {
      continue;
    }
    for (const items::Modifier& modifier : item->second.equip().modifiers()) {
      Add(player.stats, modifier);
    }
    for (const items::Modifier& roll : stack.rolls()) {
      Add(player.stats, roll);
    }
    if (slot == items::SLOT_MAIN_HAND && item->second.has_weapon()) {
      player.stats.weapon = &item->second.weapon();
    }
  }
  // Less to wear can mean less health to have, and a swing with a weapon
  // that's been put away is over.
  player.health = std::min(player.health, MaxHealthOf(player));
  if (player.motion == Motion::kAttack && player.stats.weapon != before) {
    player.motion = Motion::kIdle;
  }
}

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

absl::Status World::Learn(uint32_t id, uint32_t npc, uint32_t skill) {
  const auto player = players_.find(id);
  const auto trainer = npcs_.find(npc);
  const auto taught = content_->skills.by_id.find(skill);
  if (player == players_.end() || trainer == npcs_.end() ||
      !trainer->second.teaches.contains(skill) ||
      taught == content_->skills.by_id.end()) {
    return absl::NotFoundError("Nobody here teaches that.");
  }
  if (std::hypot(trainer->second.x - player->second.x,
                 trainer->second.z - player->second.z) > kTalkReach) {
    return absl::OutOfRangeError(absl::StrCat(
        "You're too far away to learn from the ", trainer->second.name, "."));
  }
  const std::string& name = taught->second.name();
  const uint32_t level = taught->second.level();
  if (LevelFor(player->second.experience).number < static_cast<int>(level)) {
    return absl::FailedPreconditionError(
        absl::StrCat("You can learn ", name, " from level ", level, "."));
  }
  if (player->second.skills.contains(skill)) {
    return absl::AlreadyExistsError(
        absl::StrCat("You know ", name, " already."));
  }
  player->second.skills.insert(skill);
  return absl::OkStatus();
}

absl::Status World::TakeQuest(uint32_t id, uint32_t npc, uint32_t quest) {
  const auto player = players_.find(id);
  const auto giver = npcs_.find(npc);
  const auto given = content_->quests.find(quest);
  if (player == players_.end() || giver == npcs_.end() ||
      given == content_->quests.end() ||
      given->second.giver() != giver->second.kind) {
    return absl::NotFoundError("Nobody here gives that quest.");
  }
  if (std::hypot(giver->second.x - player->second.x,
                 giver->second.z - player->second.z) > kTalkReach) {
    return absl::OutOfRangeError(absl::StrCat(
        "You're too far away to talk to the ", giver->second.name, "."));
  }
  if (player->second.quests.contains(quest)) {
    return absl::AlreadyExistsError(
        absl::StrCat("You're on ", given->second.name(), " already."));
  }
  if (player->second.quests_done.contains(quest)) {
    return absl::AlreadyExistsError(
        absl::StrCat("You've done ", given->second.name(), " already."));
  }
  player->second.quests[quest] = 0;
  ++player->second.quests_revision;
  return absl::OkStatus();
}

absl::Status World::TurnIn(uint32_t id, uint32_t npc, uint32_t quest) {
  const auto player = players_.find(id);
  const auto giver = npcs_.find(npc);
  const auto given = content_->quests.find(quest);
  if (player == players_.end() || giver == npcs_.end() ||
      given == content_->quests.end() ||
      given->second.giver() != giver->second.kind) {
    return absl::NotFoundError("Nobody here takes that quest.");
  }
  if (std::hypot(giver->second.x - player->second.x,
                 giver->second.z - player->second.z) > kTalkReach) {
    return absl::OutOfRangeError(absl::StrCat(
        "You're too far away to talk to the ", giver->second.name, "."));
  }
  const quests::Quest& done = given->second;
  const auto on = player->second.quests.find(quest);
  if (on == player->second.quests.end()) {
    return absl::NotFoundError(
        absl::StrCat("You're not on ", done.name(), "."));
  }
  if (on->second < done.kill().count()) {
    return absl::FailedPreconditionError(
        absl::StrCat(done.name(), " asks for ", done.kill().count(),
                     ", and you've killed ", on->second, "."));
  }
  // The prizes go into a copy of the bag, so that it's all of them or none.
  Inventory bag = player->second.inventory;
  std::vector<std::string> prizes;
  for (const quests::Prize& prize : done.prizes()) {
    const auto item = content_->items.by_id.find(prize.item());
    if (item == content_->items.by_id.end()) {
      continue;
    }
    if (Give(bag, prize.item(), prize.count()) < prize.count()) {
      return absl::ResourceExhaustedError(
          "Your bag is full. Make room for the prizes first.");
    }
    prizes.push_back(Some(item->second, prize.count()));
  }
  player->second.inventory = std::move(bag);
  Gain(player->second, done.experience());
  player->second.quests.erase(on);
  player->second.quests_done.insert(quest);
  ++player->second.quests_revision;
  prizes.insert(prizes.begin(), absl::StrCat(done.experience(), " experience"));
  notices_.push_back({.player = id,
                      .text = absl::StrCat("You've done ", done.name(),
                                           ", for ", List(prizes), ".")});
  return absl::OkStatus();
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
  RunDown(player.cooldowns, time);
  RunDown(player.item_cooldowns, time);
  if (player.motion == Motion::kDead) {
    player.input_skill = 0;
    player.respawn -= time;
    if (player.respawn <= std::chrono::nanoseconds::zero()) {
      player.x = 0.0f;
      player.z = 0.0f;
      player.motion = Motion::kIdle;
      player.health = MaxHealthOf(player);
    }
    return;
  }
  if (player.motion == Motion::kSkill) {
    const skills::Skill& skill = content_->skills.by_id.at(player.skill);
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
  const items::Weapon* weapon = player.stats.weapon;
  if (player.motion == Motion::kAttack && weapon != nullptr) {
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
       static_cast<int>(weapon.damage()) + ExtraDamage(level) +
           player.stats.damage);
}

const skills::Skill* World::Usable(const Player& player, uint32_t id) const {
  const auto found = content_->skills.by_id.find(id);
  if (found == content_->skills.by_id.end() || !player.skills.contains(id) ||
      player.cooldowns.contains(id)) {
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
                           MaxHealthOf(player));
  if (effect.damage() == 0) {
    return;
  }
  const int damage = static_cast<int>(effect.damage()) + ExtraDamage(level) +
                     player.stats.damage;
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
  monster.attacker = id;
  monster.health = std::max(monster.health - damage, 0);
  if (monster.health > 0) {
    return;
  }
  monster.motion = Motion::kDead;
  monster.respawn = kRespawnTime;
  Gain(player, monster.experience);
  Loot(id, player, monster);
  Count(id, player, monster);
}

void World::Loot(uint32_t id, Player& player, const Monster& monster) {
  const auto kind = content_->monsters.kinds.find(monster.kind);
  if (kind == content_->monsters.kinds.end()) {
    return;
  }
  std::vector<std::string> kept;
  std::vector<std::string> left;
  for (const monsters::Drop& drop : kind->second.loot()) {
    const auto item = content_->items.by_id.find(drop.item());
    if (item == content_->items.by_id.end() ||
        !std::bernoulli_distribution(drop.chance())(random_)) {
      continue;
    }
    const uint32_t least = std::max(drop.min(), 1u);
    const uint32_t count = std::uniform_int_distribution(
        least, std::max(drop.max(), least))(random_);
    const uint32_t fitted = Give(player.inventory, drop.item(), count);
    if (fitted > 0) {
      kept.push_back(Some(item->second, fitted));
    }
    if (fitted < count) {
      left.push_back(Some(item->second, count - fitted));
    }
  }
  if (!kept.empty()) {
    notices_.push_back({.player = id,
                        .text = absl::StrCat("The ", kind->second.name(),
                                             " dropped ", List(kept), ".")});
  }
  if (!left.empty()) {
    notices_.push_back({.player = id,
                        .text = absl::StrCat("Your bag is full, so you left ",
                                             List(left), " behind.")});
  }
}

void World::Count(uint32_t id, Player& player, const Monster& monster) {
  const auto kind = content_->monsters.kinds.find(monster.kind);
  for (auto& [quest, kills] : player.quests) {
    const auto found = content_->quests.find(quest);
    if (found == content_->quests.end() ||
        kind == content_->monsters.kinds.end()) {
      continue;
    }
    const quests::Kill& kill = found->second.kill();
    if (kill.monster() != monster.kind || kills >= kill.count()) {
      continue;
    }
    ++kills;
    ++player.quests_revision;
    std::string text =
        absl::StrCat(found->second.name(), ": ", kills, " of ", kill.count(),
                     " ", kind->second.name(), "s killed.");
    const auto giver = content_->npcs.kinds.find(found->second.giver());
    if (kills == kill.count() && giver != content_->npcs.kinds.end()) {
      absl::StrAppend(&text, " Go back to the ", giver->second.name(), ".");
    }
    notices_.push_back({.player = id, .text = std::move(text)});
  }
}

uint32_t World::Give(Inventory& bag, uint32_t item, uint32_t count) {
  const auto found = content_->items.by_id.find(item);
  if (found == content_->items.by_id.end()) {
    return 0;
  }
  // What stacks goes as one stack, and what doesn't one at a time, each with
  // rolls of its own.
  const uint32_t each = StackSize(found->second) > 1 ? count : 1;
  uint32_t fitted = 0;
  for (uint32_t made = 0; made < count; made += each) {
    items::Stack stack;
    stack.set_item(item);
    stack.set_count(each);
    for (const items::Roll& roll : found->second.rolls()) {
      items::Modifier* rolled = stack.add_rolls();
      rolled->set_stat(roll.stat());
      rolled->set_amount(
          std::uniform_int_distribution(roll.min(), roll.max())(random_));
    }
    fitted += each - bag.Add(content_->items, stack);
  }
  return fitted;
}

std::vector<Notice> World::TakeNotices() { return std::exchange(notices_, {}); }

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
  // What the player wears takes the blow, down to one.
  const int damage = std::max(monster.damage - player.stats.armour, 1);
  player.health = std::max(player.health - damage, 0);
  if (player.health > 0) {
    return;
  }
  player.motion = Motion::kDead;
  player.respawn = kRespawnTime;
}

}  // namespace nanommo
