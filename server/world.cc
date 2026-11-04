#include "server/world.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <numbers>
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

// For the monster a player picked, wherever it is.
constexpr float kAnyDistance = std::numeric_limits<float>::infinity();

// Whether the skill hurts the one monster the player picked: it does damage,
// and not to all around them. A heal doesn't.
bool OneMonster(const skills::Effect& effect) {
  return effect.damage() > 0 && !effect.all();
}

float Distance(const Player& player, const Monster& monster) {
  return std::hypot(monster.x - player.x, monster.z - player.z);
}

// Whether the monster's within the slice in front of the player, as wide as
// the arc, in degrees. Without one, it's all the way round.
bool InArc(const Player& player, const Monster& monster, float arc) {
  if (arc <= 0.0f) {
    return true;
  }
  const float toward = std::atan2(monster.x - player.x, monster.z - player.z);
  const float off =
      std::remainder(toward - player.yaw, 2.0f * std::numbers::pi_v<float>);
  return std::abs(off) <= arc / 2.0f * std::numbers::pi_v<float> / 180.0f;
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
// level. Returns whether they did.
bool Gain(Player& player, uint32_t experience) {
  const int level = LevelFor(player.experience).number;
  player.experience += experience;
  if (LevelFor(player.experience).number > level) {
    player.health = MaxHealthOf(player);
    return true;
  }
  return false;
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
             std::shared_ptr<const Content> content,
             std::shared_ptr<Paths> paths)
    : terrain_(std::move(terrain)),
      content_(std::move(content)),
      paths_(paths != nullptr ? std::move(paths)
                              : std::make_shared<Paths>(terrain_)),
      random_(seed) {}

void World::Join(uint32_t id, Player player) {
  // The monster they picked, and the way they walked, were another land's.
  player.target = 0;
  player.way.clear();
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
  Heal(id, player, static_cast<int>(use.heal()));
  if (use.cooldown_ms() > 0) {
    player.item_cooldowns[item->first] =
        std::chrono::milliseconds(use.cooldown_ms());
  }
  Event& used = Record(Event::Kind::kUse, id, player);
  used.item = item->first;
  used.count = 1;
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
  if (player.motion == Motion::kSkill && player.stats.weapon != before &&
      content_->skills.by_id.at(player.skill).effect().weapon()) {
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

void World::Pick(uint32_t id, uint32_t monster) {
  const auto found = players_.find(id);
  if (found != players_.end()) {
    found->second.target = monster;
  }
}

void World::Respawn(uint32_t id) {
  const auto found = players_.find(id);
  if (found == players_.end()) {
    return;
  }
  Player& player = found->second;
  if (player.motion != Motion::kDead ||
      player.respawn > std::chrono::nanoseconds::zero()) {
    return;
  }
  player.x = 0.0f;
  player.z = 0.0f;
  player.motion = Motion::kIdle;
  player.health = MaxHealthOf(player);
}

void World::WalkTo(uint32_t id, Place to) {
  ZoneScoped;
  const auto found = players_.find(id);
  if (found == players_.end() || found->second.motion == Motion::kDead ||
      !std::isfinite(to.x) || !std::isfinite(to.z)) {
    return;
  }
  Player& player = found->second;
  const std::vector<Place> way = paths_->Find({player.x, player.z}, to);
  player.way.assign(way.begin(), way.end());
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
  Record(Event::Kind::kTake, id, player->second).quest = quest;
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
  Record(Event::Kind::kTurnIn, id, player->second).quest = quest;
  for (const quests::Prize& prize : done.prizes()) {
    Event& given = Record(Event::Kind::kPrize, id, player->second);
    given.quest = quest;
    given.item = prize.item();
    given.count = prize.count();
  }
  if (Gain(player->second, done.experience())) {
    Record(Event::Kind::kLevel, id, player->second).level =
        static_cast<uint32_t>(LevelFor(player->second.experience).number);
  }
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
  // The dead wait, and then stay down until they ask to come back.
  if (player.motion == Motion::kDead) {
    player.input_skill = 0;
    player.respawn =
        std::max(player.respawn - time, std::chrono::nanoseconds::zero());
    return;
  }
  if (player.motion == Motion::kSkill) {
    const skills::Effect effect =
        EffectOf(player, content_->skills.by_id.at(player.skill));
    const auto before = player.cast;
    player.cast += time;
    Stride(player, effect);
    // A skill without blows of its own lands once, at its hit.
    skills::Blow once;
    once.set_at_ms(effect.hit_ms());
    once.set_push(effect.push());
    const int blows = std::max(effect.blows_size(), 1);
    for (int index = 0; index < blows; ++index) {
      const skills::Blow& blow =
          effect.blows_size() > 0 ? effect.blows(index) : once;
      const std::chrono::milliseconds hit(blow.at_ms());
      if (before < hit && player.cast >= hit) {
        Land(id, player, effect, blow, index, blows);
      }
    }
    if (player.cast < std::chrono::milliseconds(effect.cast_ms())) {
      return;
    }
    player.motion = Motion::kIdle;
    // A swing that goes on into another leaves a moment to use it.
    const skills::Skill& done = content_->skills.by_id.at(player.skill);
    player.chain = done.next();
    player.chain_left = std::chrono::milliseconds(done.window_ms());
  }
  player.chain_left =
      std::max(player.chain_left - time, std::chrono::nanoseconds::zero());
  // Space is the skill everyone has, a swing of what they hold, and the next
  // in its chain while there's time to use it.
  if (player.input_attack && player.input_skill == 0) {
    player.input_skill =
        player.chain != 0 &&
                player.chain_left > std::chrono::nanoseconds::zero()
            ? player.chain
            : content_->skills.attack;
  }
  // A skill asked for while the player was busy waits until now, and one they
  // can't use yet goes. A swing or a skill at the monster they picked, out of
  // reach, runs them up to it first, unless they walk off themselves.
  const bool walking = player.input_x != 0.0f || player.input_z != 0.0f;
  // The keys, a swing or a skill call off a walk the player asked for.
  if (walking || player.input_skill != 0) {
    player.way.clear();
  }
  if (player.input_skill != 0) {
    const skills::Skill* skill = Usable(player, player.input_skill);
    const skills::Effect effect =
        skill != nullptr ? EffectOf(player, *skill) : skills::Effect();
    const bool far = OneMonster(effect) &&
                     Target(player, kAnyDistance) != nullptr &&
                     Target(player, effect.reach()) == nullptr;
    if (skill != nullptr && !far) {
      player.input_skill = 0;
      Start(player, *skill);
      return;
    }
    if (skill != nullptr && !walking) {
      Approach(player, effect.reach(), time);
      return;
    }
    player.input_skill = 0;
  }
  if (!player.way.empty()) {
    Follow(player, time);
    return;
  }
  Move(player, time);
}

void World::Move(Player& player, std::chrono::nanoseconds time) {
  Go(player, player.input_x, player.input_z, player.input_run, time);
}

void World::Go(Player& player, float to_x, float to_z, bool run,
               std::chrono::nanoseconds time) {
  const float seconds = std::chrono::duration<float>(time).count();
  const float speed = run ? kRunSpeed : kWalkSpeed;
  const float edge = terrain_.edge();
  float x = player.x;
  float z = player.z;
  Walk(terrain_, x, z,
       std::clamp(player.x + to_x * speed * seconds, -edge, edge),
       std::clamp(player.z + to_z * speed * seconds, -edge, edge));
  // Motion comes from how far it actually went, so that a player pushing
  // against the edge or the sea stands still, instead of walking on the spot.
  const float dx = x - player.x;
  const float dz = z - player.z;
  if (dx == 0.0f && dz == 0.0f) {
    player.motion = Motion::kIdle;
  } else {
    player.motion = run ? Motion::kRun : Motion::kWalk;
    player.yaw = std::atan2(dx, dz);
  }
  player.x = x;
  player.z = z;
}

void World::Follow(Player& player, std::chrono::nanoseconds time) {
  const Place next = player.way.front();
  const float distance = std::hypot(next.x - player.x, next.z - player.z);
  const float step = kRunSpeed * std::chrono::duration<float>(time).count();
  if (distance > 0.0f) {
    // The last step to each place is shorter, so that it stops there.
    const float share = std::min(distance / step, 1.0f);
    const float x = player.x;
    const float z = player.z;
    Go(player, (next.x - x) / distance * share, (next.z - z) / distance * share,
       true, time);
    // Something the squares missed is in the way after all: they stop.
    if (player.x == x && player.z == z) {
      player.way.clear();
      return;
    }
  }
  if (distance <= step) {
    player.way.pop_front();
  }
}

void World::Approach(Player& player, float reach,
                     std::chrono::nanoseconds time) {
  const Monster* monster = Target(player, kAnyDistance);
  const float distance = Distance(player, *monster);
  const float step = kRunSpeed * std::chrono::duration<float>(time).count();
  // The last step is shorter, so that it stops within the reach.
  const float share =
      std::clamp((distance - reach * kCloseIn) / step, 0.0f, 1.0f);
  Go(player, (monster->x - player.x) / distance * share,
     (monster->z - player.z) / distance * share, true, time);
}

const Monster* World::Target(const Player& player, float reach) const {
  const auto found = monsters_.find(player.target);
  if (found == monsters_.end() || found->second.motion == Motion::kDead ||
      Distance(player, found->second) > reach) {
    return nullptr;
  }
  return &found->second;
}

// Turns the player towards the monster they picked, if it's within reach.
// Without one, they swing at the air the way they face.
void World::Aim(Player& player, float reach) {
  if (const Monster* monster = Target(player, reach)) {
    player.yaw = std::atan2(monster->x - player.x, monster->z - player.z);
  }
}

const skills::Skill* World::Usable(const Player& player, uint32_t id) const {
  const auto found = content_->skills.by_id.find(id);
  if (found == content_->skills.by_id.end() || !player.skills.contains(id) ||
      player.cooldowns.contains(id)) {
    return nullptr;
  }
  const skills::Effect& effect = found->second.effect();
  if (effect.weapon()
          ? player.stats.weapon == nullptr
          : OneMonster(effect) && Target(player, kAnyDistance) == nullptr) {
    return nullptr;
  }
  return &found->second;
}

skills::Effect World::EffectOf(const Player& player,
                               const skills::Skill& skill) const {
  skills::Effect effect = skill.effect();
  const items::Weapon* weapon = player.stats.weapon;
  if (effect.weapon() && weapon != nullptr) {
    const float pace = effect.pace() > 0.0f ? effect.pace() : 1.0f;
    auto paced = [pace](uint32_t whole) {
      return static_cast<uint32_t>(
          std::lround(pace * static_cast<float>(whole)));
    };
    effect.set_reach(weapon->reach());
    effect.set_damage(std::max(paced(weapon->damage()), 1u));
    effect.set_cast_ms(paced(weapon->swing_ms()));
    effect.set_hit_ms(paced(weapon->hit_ms()));
  }
  return effect;
}

void World::Start(Player& player, const skills::Skill& skill) {
  player.motion = Motion::kSkill;
  player.skill = skill.id();
  player.cast = std::chrono::nanoseconds::zero();
  // Any skill uses up the chain, which the swing's end starts again.
  player.chain = 0;
  ++player.casts;
  if (skill.cooldown_ms() > 0) {
    player.cooldowns[skill.id()] =
        std::chrono::milliseconds(skill.cooldown_ms());
  }
  Aim(player, EffectOf(player, skill).reach());
  player.cast_x = player.x;
  player.cast_z = player.z;
}

// Along the way they faced as it started, which Aim turned them to, and like
// a walk where something's in the way.
void World::Stride(Player& player, const skills::Effect& effect) {
  if (effect.strides_size() == 0) {
    return;
  }
  const float ahead = AheadAt(
      effect, std::chrono::duration<float, std::milli>(player.cast).count());
  const float edge = terrain_.edge();
  Walk(terrain_, player.x, player.z,
       std::clamp(player.cast_x + std::sin(player.yaw) * ahead, -edge, edge),
       std::clamp(player.cast_z + std::cos(player.yaw) * ahead, -edge, edge));
}

void World::Land(uint32_t id, Player& player, const skills::Effect& effect,
                 const skills::Blow& blow, int index, int blows) {
  const int level = LevelFor(player.experience).number;
  if (index == 0) {
    Heal(id, player, static_cast<int>(effect.heal()));
  }
  if (effect.damage() == 0) {
    return;
  }
  // The blows share the damage, and the last takes what doesn't divide. A
  // quick swing takes its share of what the level and the gear add, as it does
  // of the weapon's.
  const float pace =
      effect.weapon() && effect.pace() > 0.0f ? effect.pace() : 1.0f;
  const int total =
      static_cast<int>(effect.damage()) +
      static_cast<int>(std::lround(
          pace * static_cast<float>(ExtraDamage(level) + player.stats.damage)));
  const int damage = total / blows + (index == blows - 1 ? total % blows : 0);
  for (auto& [monster_id, monster] : monsters_) {
    if ((effect.all() || monster_id == player.target) &&
        monster.motion != Motion::kDead &&
        Distance(player, monster) <= effect.reach() &&
        InArc(player, monster, effect.arc())) {
      Push(player, monster, blow.push());
      Throw(monster, blow.lift());
      Hurt(id, player, monster_id, monster, damage);
    }
  }
}

// Away from the player. A push that comes while another is going on starts
// again from where the monster is.
void World::Push(const Player& player, Monster& monster, float metres) {
  const float distance = Distance(player, monster);
  if (metres == 0.0f || distance == 0.0f) {
    return;
  }
  monster.pushed_from_x = monster.x;
  monster.pushed_from_z = monster.z;
  monster.push_x = (monster.x - player.x) / distance * metres;
  monster.push_z = (monster.z - player.z) / distance * metres;
  monster.pushed = std::chrono::nanoseconds::zero();
}

// Like a walk: where the sea or a tree is in the way, it slides along it, or
// stays put. Each tick goes to where the push has reached by then, eased out,
// so that it's fast at first and slows to a stop at its whole length.
bool World::Slide(Monster& monster, std::chrono::nanoseconds time) {
  if (monster.pushed >= kPushTime) {
    return false;
  }
  monster.pushed =
      std::min<std::chrono::nanoseconds>(monster.pushed + time, kPushTime);
  const float done = std::chrono::duration<float>(monster.pushed) /
                     std::chrono::duration<float>(kPushTime);
  const float eased = 1.0f - (1.0f - done) * (1.0f - done);
  const float edge = terrain_.edge();
  Walk(terrain_, monster.x, monster.z,
       std::clamp(monster.pushed_from_x + monster.push_x * eased, -edge, edge),
       std::clamp(monster.pushed_from_z + monster.push_z * eased, -edge, edge));
  return true;
}

// As a ball thrown straight up: fast at first, slowing to the top, and back
// down.
void World::Throw(Monster& monster, float lift) {
  const bool flying = monster.height > 0.0f || monster.rise > 0.0f;
  // Long enough up, or just down, nothing throws it up again, though a blow
  // can still dash it down.
  const bool spent = flying ? monster.in_air >= kMostInAir
                            : monster.steady > std::chrono::nanoseconds::zero();
  if (spent && lift >= 0.0f) {
    return;
  }
  // Up to the lift's height off the ground, however high it is already, so
  // that blow after blow doesn't send it ever higher.
  if (lift > monster.height) {
    monster.rise = std::max(
        monster.rise, std::sqrt(2.0f * kGravity * (lift - monster.height)));
  } else if (lift < 0.0f && flying) {
    monster.rise = -std::sqrt(2.0f * kGravity * -lift);
  } else if (flying) {
    monster.rise = std::max(monster.rise, kPop);
  } else {
    return;
  }
  // Thrown, it drops the swing it was in the middle of.
  if (monster.motion == Motion::kAttack) {
    monster.motion = Motion::kIdle;
  }
}

bool World::Fly(Monster& monster, std::chrono::nanoseconds time) {
  if (monster.height <= 0.0f && monster.rise <= 0.0f) {
    monster.steady =
        std::max(monster.steady - time, std::chrono::nanoseconds::zero());
    return false;
  }
  monster.in_air += time;
  // Its time up there's over: it goes no higher.
  if (monster.in_air >= kMostInAir) {
    monster.rise = std::min(monster.rise, 0.0f);
  }
  const float seconds = std::chrono::duration<float>(time).count();
  monster.height += (monster.rise - kGravity * seconds / 2.0f) * seconds;
  monster.rise -= kGravity * seconds;
  if (monster.height <= 0.0f) {
    monster.height = 0.0f;
    monster.rise = 0.0f;
    monster.in_air = std::chrono::nanoseconds::zero();
    monster.steady = kSteady;
    return false;
  }
  return true;
}

void World::Hurt(uint32_t id, Player& player, uint32_t monster_id,
                 Monster& monster, int damage) {
  monster.attacker = id;
  monster.health = std::max(monster.health - damage, 0);
  Event& hit = Record(Event::Kind::kHit, id, player);
  hit.monster = monster.kind;
  hit.monster_id = monster_id;
  hit.amount = static_cast<uint32_t>(damage);
  if (monster.health > 0) {
    return;
  }
  monster.motion = Motion::kDead;
  monster.respawn = kRespawnTime;
  Record(Event::Kind::kKill, id, player).monster = monster.kind;
  if (Gain(player, monster.experience)) {
    Record(Event::Kind::kLevel, id, player).level =
        static_cast<uint32_t>(LevelFor(player.experience).number);
  }
  Loot(id, player, monster);
  Count(id, player, monster);
}

void World::Heal(uint32_t id, Player& player, int health) {
  const int healed = std::min(health, MaxHealthOf(player) - player.health);
  if (healed <= 0) {
    return;
  }
  player.health += healed;
  Record(Event::Kind::kHeal, id, player).amount = static_cast<uint32_t>(healed);
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
      Event& looted = Record(Event::Kind::kLoot, id, player);
      looted.monster = monster.kind;
      looted.item = drop.item();
      looted.count = fitted;
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

std::vector<Event> World::TakeEvents() { return std::exchange(events_, {}); }

Event& World::Record(Event::Kind kind, uint32_t id, const Player& player) {
  return events_.emplace_back(Event{.kind = kind,
                                    .player = player.name,
                                    .player_id = id,
                                    .land = player.land,
                                    .x = player.x,
                                    .z = player.z});
}

void World::Act(Monster& monster, std::chrono::nanoseconds time) {
  if (monster.motion == Motion::kDead) {
    // A push the killing blow started ends with the monster, but one thrown
    // up still comes down.
    monster.pushed = kPushTime;
    Fly(monster, time);
    Revive(monster, time, random_);
    return;
  }
  // Pushed, it goes where the blow sends it, and a swing it started goes on.
  const bool pushed = Slide(monster, time);
  if (Fly(monster, time)) {
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
  if (pushed) {
    monster.motion = Motion::kIdle;
    return;
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
  Event& hurt = Record(Event::Kind::kHurt, found->first, player);
  hurt.monster = monster.kind;
  hurt.amount = static_cast<uint32_t>(damage);
  if (player.health > 0) {
    return;
  }
  player.motion = Motion::kDead;
  player.respawn = kRespawnTime;
  player.way.clear();
  Record(Event::Kind::kDeath, found->first, player).monster = monster.kind;
}

}  // namespace nanommo
