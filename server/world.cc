#include "server/world.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
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

float Distance(const Player& a, const Player& b) {
  return std::hypot(b.x - a.x, b.z - a.z);
}

float Distance(const Player& player, Place place) {
  return std::hypot(place.x - player.x, place.z - player.z);
}

// Where the unit is, a monster or a player, by its id among them, while it's
// alive and within reach of the player, or nothing.
template <typename Unit>
std::optional<Place> Within(const std::map<uint32_t, Unit>& units, uint32_t id,
                            const Player& player, float reach) {
  const auto found = units.find(id);
  if (found == units.end() || found->second.motion == Motion::kDead ||
      Distance(player, found->second) > reach) {
    return std::nullopt;
  }
  return Place{.x = found->second.x, .z = found->second.z};
}

// What's offered in a trade stays where it is until the trade's over, as the
// trade takes it from there.
constexpr std::string_view kOffered =
    "You can't, while it's offered in your trade.";

// Whether the unit, a monster or a duel's opponent, is within the slice in
// front of the player, as wide as the arc, in degrees. Without one, it's all
// the way round.
template <typename Unit>
bool InArc(const Player& player, const Unit& unit, float arc) {
  if (arc <= 0.0f) {
    return true;
  }
  const float toward = std::atan2(unit.x - player.x, unit.z - player.z);
  const float off =
      std::remainder(toward - player.yaw, 2.0f * std::numbers::pi_v<float>);
  return std::abs(off) <= arc / 2.0f * std::numbers::pi_v<float> / 180.0f;
}

// Whether the monster's health has fallen to the phase's share of its most.
bool FallenTo(const Monster& monster, const monsters::Phase& phase) {
  return monster.health * 100 <=
         monster.max_health * static_cast<int>(phase.health());
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
// level. Returns whether they did. A boss's helpers who died in the fight get
// their share too, and come back at full health anyway.
bool Gain(Player& player, uint32_t experience) {
  const int level = LevelFor(player.experience).number;
  player.experience += experience;
  if (LevelFor(player.experience).number > level) {
    if (player.motion != Motion::kDead) {
      player.health = MaxHealthOf(player);
    }
    return true;
  }
  return false;
}

// "30 seconds", "a minute" or "2 minutes".
std::string InWords(std::chrono::nanoseconds time) {
  const auto seconds =
      std::chrono::duration_cast<std::chrono::seconds>(time).count();
  if (seconds % 60 != 0) {
    return absl::StrCat(seconds, " seconds");
  }
  return seconds == 60 ? "a minute" : absl::StrCat(seconds / 60, " minutes");
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

World::World(uint32_t seed, Terrain terrain,
             std::shared_ptr<const Content> content,
             std::shared_ptr<Paths> paths)
    : terrain_(std::move(terrain)),
      content_(std::move(content)),
      paths_(paths != nullptr ? std::move(paths)
                              : std::make_shared<Paths>(terrain_)),
      random_(seed) {}

void World::Join(uint32_t id, Player player) {
  // What they picked, and the way they walked, were another land's.
  player.target = 0;
  player.target_player = 0;
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
  // The kit's items get their serials here, and so do those from before
  // serials, the first time they're back.
  player.inventory.Number(items);
  Dress(player);
  // Zero when none was saved, as for a player from before we kept health.
  if (player.health <= 0) {
    player.health = MaxHealthOf(player);
  }
  player.skills.insert(content_->skills.starting.begin(),
                       content_->skills.starting.end());
  players_[id] = std::move(player);
}

void World::Leave(uint32_t id) {
  // Both have to be here to trade, or to duel.
  if (const uint32_t other = trades_.End(id); other != 0) {
    notices_.push_back(
        {.player = other,
         .text = absl::StrCat(players_.at(id).name,
                              " has gone, so the trade's off.")});
  }
  if (const uint32_t other = duels_.End(id); other != 0) {
    notices_.push_back({.player = other,
                        .text = absl::StrCat(players_.at(id).name,
                                             " has gone, so the duel's off.")});
  }
  players_.erase(id);
  history_.Leave(id);
}

absl::Status World::MoveItem(uint32_t id, const protocol::Place& from,
                             const protocol::Place& to, uint32_t count) {
  const auto found = players_.find(id);
  if (found == players_.end()) {
    return absl::NotFoundError("There's nobody to move it.");
  }
  if (trades_.Holds(id, from) || trades_.Holds(id, to)) {
    return absl::FailedPreconditionError(kOffered);
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
  if (trades_.Holds(id, place)) {
    return absl::FailedPreconditionError(kOffered);
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
  if (trades_.Holds(id, place)) {
    return absl::FailedPreconditionError(kOffered);
  }
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

absl::Status World::AskToTrade(uint32_t id, std::string_view name) {
  const auto player = players_.find(id);
  const auto other = std::ranges::find_if(players_, [name](const auto& entry) {
    return absl::EqualsIgnoreCase(entry.second.name, name);
  });
  if (player == players_.end() || other == players_.end()) {
    return absl::NotFoundError("Nobody of that name is here.");
  }
  if (other == player) {
    return absl::InvalidArgumentError("You can't trade with yourself.");
  }
  const std::string& asker = player->second.name;
  const std::string& asked = other->second.name;
  if (Distance(player->second, other->second) > kTradeReach) {
    return absl::OutOfRangeError(
        absl::StrCat("You're too far away to trade with ", asked, "."));
  }
  if (trades_.With(id) != 0) {
    return absl::FailedPreconditionError("You're trading already.");
  }
  if (trades_.With(other->first) != 0) {
    return absl::FailedPreconditionError(
        absl::StrCat(asked, " is trading with someone already."));
  }
  if (!trades_.Ask(id, other->first)) {
    notices_.push_back(
        {.player = other->first,
         .text = absl::StrCat(asker, " asks to trade. Type /trade ", asker,
                              " to say yes.")});
    notices_.push_back(
        {.player = id,
         .text = absl::StrCat("You asked ", asked, " to trade.")});
  }
  return absl::OkStatus();
}

absl::Status World::OfferToTrade(uint32_t id,
                                 const protocol::TradeOffer& offer) {
  const auto player = players_.find(id);
  if (player == players_.end()) {
    return absl::NotFoundError("There's nobody to trade.");
  }
  return trades_.Offer(id, player->second.inventory, content_->items, offer);
}

absl::StatusOr<uint32_t> World::ReadyToTrade(uint32_t id, bool ready,
                                             uint32_t revision) {
  const uint32_t other = trades_.With(id);
  if (other == 0) {
    return absl::FailedPreconditionError("You're not trading with anyone.");
  }
  Player& player = players_.at(id);
  Player& them = players_.at(other);
  if (ready && Distance(player, them) > kTradeReach) {
    return absl::OutOfRangeError(
        absl::StrCat("You're too far away to trade with ", them.name, "."));
  }
  const absl::StatusOr<bool> both = trades_.Ready(id, ready, revision);
  if (!both.ok()) {
    return both.status();
  }
  if (!*both) {
    return 0;
  }
  // Both are ready, so they trade, here and now.
  const absl::Status traded =
      trades_.Complete(id, player.inventory, them.inventory, content_->items);
  if (!traded.ok()) {
    notices_.push_back(
        {.player = other, .text = std::string(traded.message())});
    return traded;
  }
  notices_.push_back(
      {.player = id, .text = absl::StrCat("You traded with ", them.name, ".")});
  notices_.push_back(
      {.player = other,
       .text = absl::StrCat("You traded with ", player.name, ".")});
  return other;
}

void World::CancelTrade(uint32_t id) {
  if (const uint32_t other = trades_.End(id); other != 0) {
    notices_.push_back(
        {.player = other,
         .text = absl::StrCat(players_.at(id).name, " called the trade off.")});
  }
}

protocol::Trade World::TradeOf(uint32_t id) const {
  protocol::Trade trade;
  if (const uint32_t other = trades_.With(id); other != 0) {
    trade.set_partner(players_.at(other).name);
    *trade.mutable_yours() = trades_.SideOf(id);
    *trade.mutable_theirs() = trades_.SideOf(other);
    trade.set_revision(trades_.RevisionOf(id));
  }
  return trade;
}

std::set<uint32_t> World::TakeTradeNews() { return trades_.TakeNews(); }

absl::Status World::AskToDuel(uint32_t id, std::string_view name) {
  const auto player = players_.find(id);
  const auto other = std::ranges::find_if(players_, [name](const auto& entry) {
    return absl::EqualsIgnoreCase(entry.second.name, name);
  });
  if (player == players_.end() || other == players_.end()) {
    return absl::NotFoundError("Nobody of that name is here.");
  }
  if (other == player) {
    return absl::InvalidArgumentError("You can't duel yourself.");
  }
  // Asking back the one who asked is a yes.
  if (const Duels::Duel* duel = duels_.Of(id);
      duel != nullptr && duel->state == Duels::State::kAsked &&
      duel->challenged == id && duel->challenger == other->first) {
    return AnswerDuel(id, true);
  }
  const std::string& asker = player->second.name;
  const std::string& asked = other->second.name;
  if (Distance(player->second, other->second) > Duels::kReach) {
    return absl::OutOfRangeError(
        absl::StrCat("You're too far away to duel ", asked, "."));
  }
  if (duels_.Of(id) != nullptr) {
    return absl::FailedPreconditionError("You're in a duel already.");
  }
  if (duels_.Of(other->first) != nullptr) {
    return absl::FailedPreconditionError(
        absl::StrCat(asked, " is in a duel already."));
  }
  duels_.Ask(id, other->first);
  notices_.push_back(
      {.player = other->first,
       .text = absl::StrCat(asker,
                            " asks you to duel. Type /duel yes to say yes.")});
  notices_.push_back(
      {.player = id, .text = absl::StrCat("You asked ", asked, " to duel.")});
  return absl::OkStatus();
}

absl::Status World::AnswerDuel(uint32_t id, bool yes) {
  const Duels::Duel* duel = duels_.Of(id);
  if (duel == nullptr || duel->challenged != id ||
      duel->state != Duels::State::kAsked) {
    return absl::NotFoundError("Nobody's asked you to duel.");
  }
  const uint32_t asker = duel->challenger;
  const Player& challenger = players_.at(asker);
  const Player& player = players_.at(id);
  if (!yes) {
    duels_.End(id);
    notices_.push_back(
        {.player = asker,
         .text = absl::StrCat(player.name, " said no to a duel.")});
    return absl::OkStatus();
  }
  // They may have walked apart while the ask waited.
  if (Distance(player, challenger) > Duels::kReach) {
    return absl::OutOfRangeError(
        absl::StrCat("You're too far away to duel ", challenger.name, "."));
  }
  duels_.Accept(id, {.x = challenger.x, .z = challenger.z});
  return absl::OkStatus();
}

absl::Status World::Yield(uint32_t id) {
  const Duels::Duel* duel = duels_.Of(id);
  if (duel == nullptr) {
    return absl::NotFoundError("You're not in a duel.");
  }
  if (duel->state == Duels::State::kFighting) {
    Lose(id);
    return absl::OkStatus();
  }
  const uint32_t other = duels_.End(id);
  notices_.push_back(
      {.player = other,
       .text = absl::StrCat(players_.at(id).name, " called the duel off.")});
  return absl::OkStatus();
}

uint32_t World::CallOffDuel(uint32_t id) { return duels_.End(id); }

protocol::Duel World::DuelOf(uint32_t id) const {
  protocol::Duel shown;
  const Duels::Duel* duel = duels_.Of(id);
  if (duel == nullptr) {
    return shown;
  }
  const uint32_t other = duel->Against(id);
  shown.set_opponent(players_.at(other).name);
  shown.set_opponent_id(other);
  switch (duel->state) {
    case Duels::State::kAsked:
      shown.set_state(duel->challenger == id ? protocol::DUEL_ASKING
                                             : protocol::DUEL_ASKED);
      return shown;
    case Duels::State::kCountdown:
      shown.set_state(protocol::DUEL_COUNTDOWN);
      shown.set_countdown(static_cast<uint32_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(duel->left)
              .count()));
      break;
    case Duels::State::kFighting:
      shown.set_state(protocol::DUEL_FIGHTING);
      break;
  }
  shown.mutable_flag()->set_x(duel->flag.x);
  shown.mutable_flag()->set_z(duel->flag.z);
  return shown;
}

std::set<uint32_t> World::TakeDuelNews() { return duels_.TakeNews(); }

void World::Dress(Player& player) const {
  const items::Weapon* before = player.stats.weapon;
  // Whether the skill they're using has landed its first blow, with the
  // weapon they held for it.
  bool landed = false;
  if (player.motion == Motion::kSkill) {
    const skills::Effect& effect = player.effect;
    const uint32_t first =
        effect.blows_size() > 0 ? effect.blows(0).at_ms() : effect.hit_ms();
    landed = player.cast >= std::chrono::milliseconds(first);
  }
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
  // that's been put away is over, before its blow lands. After, it plays out,
  // with the effect it started with, as putting the weapon away and back
  // would end it early, for the next swing to start sooner.
  player.health = std::min(player.health, MaxHealthOf(player));
  if (player.motion == Motion::kSkill && player.stats.weapon != before &&
      !landed && content_->skills.by_id.at(player.skill).effect().weapon()) {
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

void World::Pick(uint32_t id, uint32_t monster, uint32_t player) {
  const auto found = players_.find(id);
  if (found == players_.end()) {
    return;
  }
  // Their opponent goes in place of a monster, and anyone else is nobody.
  const bool opponent = player != 0 && player == duels_.Opponent(id);
  found->second.target = opponent ? 0 : monster;
  found->second.target_player = opponent ? player : 0;
}

void World::SetBehind(uint32_t id, std::chrono::nanoseconds behind) {
  const auto found = players_.find(id);
  if (found != players_.end()) {
    found->second.behind = behind;
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
  // The time this step takes the world to, as the worlds sent after it will
  // show it.
  clock_ += time;
  for (auto& [id, player] : players_) {
    Act(id, player, time);
  }
  // Called up while the blows land, adds would be hit by the blow that called
  // them, so they come once they've all landed.
  for (const auto& [boss, phase] : std::exchange(calls_, {})) {
    CallAdds(boss, phase);
  }
  for (auto& [id, monster] : monsters_) {
    Act(monster, time);
  }
  // An add goes once its boss is back in its first phase, at home or alive
  // again, so that the next fight starts without it.
  std::erase_if(monsters_, [this](const auto& entry) {
    const uint32_t boss = entry.second.called_by;
    if (boss == 0) {
      return false;
    }
    const auto found = monsters_.find(boss);
    return found == monsters_.end() ||
           (found->second.motion != Motion::kDead && found->second.phase == 0);
  });
  Referee(time);
  for (const auto& [id, player] : players_) {
    history_.Record(id, clock_, {.x = player.x, .z = player.z});
  }
}

void World::Act(uint32_t id, Player& player, std::chrono::nanoseconds time) {
  // Cooldowns run down whatever the player's doing.
  RunDown(player.cooldowns, time);
  RunDown(player.item_cooldowns, time);
  // A skill moves them as it goes, and the dead don't move.
  player.steered =
      player.motion == Motion::kSkill || player.motion == Motion::kDead;
  // The dead wait, and then stay down until they ask to come back.
  if (player.motion == Motion::kDead) {
    player.input_skill = 0;
    player.respawn =
        std::max(player.respawn - time, std::chrono::nanoseconds::zero());
    return;
  }
  if (player.motion == Motion::kSkill) {
    const skills::Effect& effect = player.effect;
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
      const bool armed =
          !effect.weapon() || player.stats.weapon == player.cast_weapon;
      if (before < hit && player.cast >= hit && armed) {
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
                     Target(player, kAnyDistance).has_value() &&
                     !Target(player, effect.reach()).has_value();
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
  float x = player.x;
  float z = player.z;
  WalkAlong(terrain_, x, z, to_x, to_z, run,
            std::chrono::duration<float>(time).count());
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
  player.steered = true;
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
  player.steered = true;
  const std::optional<Place> to = Target(player, kAnyDistance);
  if (!to) {
    return;
  }
  const float distance = std::hypot(to->x - player.x, to->z - player.z);
  const float step = kRunSpeed * std::chrono::duration<float>(time).count();
  // The last step is shorter, so that it stops within the reach.
  const float share =
      std::clamp((distance - reach * kCloseIn) / step, 0.0f, 1.0f);
  Go(player, (to->x - player.x) / distance * share,
     (to->z - player.z) / distance * share, true, time);
}

std::optional<Place> World::Target(const Player& player, float reach) const {
  if (player.target_player == 0) {
    return Within(monsters_, player.target, player, reach);
  }
  // A duel opponent is where the player saw them, as for their blows, so
  // that's what they aim at, and run up to.
  if (!Within(players_, player.target_player, player, kAnyDistance)) {
    return std::nullopt;
  }
  const Place seen = Seen(player, player.target_player);
  if (Distance(player, seen) > reach) {
    return std::nullopt;
  }
  return seen;
}

Place World::Seen(const Player& player, uint32_t other) const {
  if (player.behind > std::chrono::nanoseconds::zero()) {
    if (const std::optional<Place> then =
            history_.At(other, clock_ - player.behind)) {
      return *then;
    }
  }
  const Player& them = players_.at(other);
  return {.x = them.x, .z = them.z};
}

// Turns the player towards what they picked, if it's within reach. Without
// it, they swing at the air the way they face.
void World::Aim(Player& player, float reach) {
  if (const std::optional<Place> at = Target(player, reach)) {
    player.yaw = std::atan2(at->x - player.x, at->z - player.z);
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
          : OneMonster(effect) && !Target(player, kAnyDistance).has_value()) {
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
  player.effect = EffectOf(player, skill);
  player.cast_weapon = player.stats.weapon;
  if (skill.cooldown_ms() > 0) {
    player.cooldowns[skill.id()] =
        std::chrono::milliseconds(skill.cooldown_ms());
  }
  Aim(player, player.effect.reach());
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
  // A boss on its way home can't be hurt, so that nobody wears it down from
  // past its leash.
  for (auto& [monster_id, monster] : monsters_) {
    if ((effect.all() || monster_id == player.target) &&
        monster.motion != Motion::kDead && !monster.evading &&
        Distance(player, monster) <= effect.reach() &&
        InArc(player, monster, effect.arc())) {
      // A boss stands its ground: a crowd's blows would shove it about, and
      // out of its own leash.
      if (!monster.boss()) {
        Push(player, monster, blow.push());
        Throw(monster, blow.lift());
      }
      Hurt(id, player, monster_id, monster, damage);
    }
  }
  // In a duel, their opponent too, when it's for them or all around, but a
  // player isn't pushed or thrown, as their own keys move them. A player can
  // run from a blow as fast as the one who swung, so it's checked against
  // where the player's client drew them, a moment in the past. A monster's hit
  // where it is, above, as it's pushed and thrown from there: it stands still
  // to swing, and comes to whoever hurt it, so where a client drew it a moment
  // ago is near enough.
  if (const uint32_t other = duels_.Opponent(id); other != 0) {
    Player& them = players_.at(other);
    const Place seen = Seen(player, other);
    if ((effect.all() || other == player.target_player) &&
        them.motion != Motion::kDead &&
        Distance(player, seen) <= effect.reach() &&
        InArc(player, seen, effect.arc())) {
      Hurt(other, them, damage);
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
  // Thrown, it drops the swing or the skill it was in the middle of.
  if (monster.motion == Motion::kAttack || monster.motion == Motion::kSkill) {
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
  // What the blow takes is the player's threat, which the monster goes by.
  const int taken = std::min(damage, monster.health);
  monster.threat[id] += taken;
  monster.health -= taken;
  Event& hit = Record(Event::Kind::kHit, id, player);
  hit.monster = monster.kind;
  hit.monster_id = monster_id;
  hit.amount = static_cast<uint32_t>(damage);
  if (monster.health > 0) {
    // A boss goes into each phase its health has fallen to, in turn.
    while (monster.phase + 1 < monster.phases.size() &&
           FallenTo(monster, monster.phases[monster.phase + 1])) {
      ++monster.phase;
      calls_.emplace_back(monster_id, monster.phase);
    }
    return;
  }
  monster.motion = Motion::kDead;
  monster.respawn = monster.respawn_time;
  // A boss's adds fall with it.
  for (auto& [add_id, add] : monsters_) {
    if (add.called_by == monster_id) {
      add.health = 0;
      add.motion = Motion::kDead;
    }
  }
  if (!monster.boss()) {
    Reward(id, player, monster);
    return;
  }
  // A boss takes a crowd, so it's everyone's who took enough of its health,
  // and not the last blow's, each with a roll of their own, and the land hears
  // who took the most.
  const Player* most = nullptr;
  int taken_most = 0;
  int helped = 0;
  for (const auto& [helper, threat] : monster.threat) {
    const auto found = players_.find(helper);
    if (found == players_.end() ||
        threat * 100 < monster.max_health * kEnoughHelp) {
      continue;
    }
    Reward(helper, found->second, monster);
    ++helped;
    if (threat > taken_most) {
      most = &found->second;
      taken_most = threat;
    }
  }
  std::string fallen = "has fallen";
  if (most != nullptr) {
    absl::StrAppend(&fallen, " to ", most->name);
  }
  if (helped > 1) {
    absl::StrAppend(&fallen, " and ", helped - 1,
                    helped == 2 ? " other" : " others");
  }
  Announce(monster, absl::StrCat(fallen, ". It rises again in ",
                                 InWords(monster.respawn_time), "."));
}

void World::Reward(uint32_t id, Player& player, const Monster& monster) {
  Record(Event::Kind::kKill, id, player).monster = monster.kind;
  if (Gain(player, monster.experience)) {
    Record(Event::Kind::kLevel, id, player).level =
        static_cast<uint32_t>(LevelFor(player.experience).number);
  }
  Loot(id, player, monster);
  Count(id, player, monster);
}

void World::Hurt(uint32_t id, Player& player, int damage) {
  // Nobody dies in a duel: the blow that would leave them nothing leaves them
  // one, and they've lost.
  const int taken = std::min(damage, player.health - 1);
  if (taken > 0) {
    player.health -= taken;
    Record(Event::Kind::kHurt, id, player).amount =
        static_cast<uint32_t>(taken);
  }
  if (taken < damage) {
    Lose(id);
  }
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
  // Gold takes no slot, so a full bag takes it too.
  if (const monsters::Gold& gold = kind->second.gold(); gold.max() > 0) {
    const uint32_t coins =
        std::uniform_int_distribution(gold.min(), gold.max())(random_);
    if (const uint32_t carried = coins - player.inventory.AddGold(coins);
        carried > 0) {
      kept.push_back(absl::StrCat(carried, " gold"));
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
  // rolls and a serial of its own.
  const bool stacks = StackSize(found->second) > 1;
  const uint32_t each = stacks ? count : 1;
  uint32_t fitted = 0;
  for (uint32_t made = 0; made < count; made += each) {
    items::Stack stack;
    stack.set_item(item);
    stack.set_count(each);
    if (!stacks) {
      stack.set_serial(NewSerial());
    }
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

void World::Referee(std::chrono::nanoseconds time) {
  for (const Duels::Duel& asked : duels_.Step(time)) {
    notices_.push_back(
        {.player = asked.challenger,
         .text = absl::StrCat(players_.at(asked.challenged).name,
                              " didn't answer, so there's no duel.")});
  }
  // Losing ends a duel, so the losers are found first, and lose after.
  std::vector<uint32_t> losers;
  for (const auto& [challenger, duel] : duels_.all()) {
    if (duel.state != Duels::State::kFighting) {
      continue;
    }
    for (const uint32_t side : {duel.challenger, duel.challenged}) {
      const Player& player = players_.at(side);
      const bool strayed =
          std::hypot(player.x - duel.flag.x, player.z - duel.flag.z) >
          Duels::kFlagReach;
      if (strayed) {
        notices_.push_back(
            {.player = side, .text = "You went too far from the flag."});
      }
      if (strayed || player.motion == Motion::kDead) {
        losers.push_back(side);
        break;
      }
    }
  }
  for (const uint32_t loser : losers) {
    Lose(loser);
  }
}

void World::Lose(uint32_t id) {
  const uint32_t winner = duels_.End(id);
  // Neither can hit the other any more.
  players_.at(id).target_player = 0;
  players_.at(winner).target_player = 0;
  notices_.push_back(
      {.player = id,
       .text = absl::StrCat(players_.at(winner).name, " won the duel.")});
  notices_.push_back({.player = winner,
                      .text = absl::StrCat("You won the duel against ",
                                           players_.at(id).name, ".")});
}

void World::Act(Monster& monster, std::chrono::nanoseconds time) {
  // As a player's, its cooldowns run down whatever it's doing.
  RunDown(monster.cooldowns, time);
  if (monster.motion == Motion::kDead) {
    // A push the killing blow started ends with the monster, but one thrown
    // up still comes down.
    monster.pushed = kPushTime;
    Fly(monster, time);
    // An add doesn't come back. It goes with its boss.
    if (monster.called_by != 0) {
      return;
    }
    const std::chrono::nanoseconds before = monster.respawn;
    Revive(monster, time, random_);
    // The land hears that a boss is about to come back, and that it has.
    if (monster.boss() && monster.motion != Motion::kDead) {
      Announce(monster, "has risen.");
    } else if (monster.boss() && before > kRisesSoon &&
               monster.respawn <= kRisesSoon) {
      Announce(monster,
               absl::StrCat("rises again in ", InWords(kRisesSoon), "."));
    }
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
  // A push doesn't stop a skill either, and it lands where it was aimed.
  if (monster.motion == Motion::kSkill) {
    monster.cast += time;
    if (monster.cast <
        std::chrono::milliseconds(monster.casting.telegraph_ms())) {
      return;
    }
    LandSkill(monster);
    monster.motion = Motion::kIdle;
  }
  if (pushed) {
    monster.motion = Motion::kIdle;
    return;
  }
  Hunt(monster, players_, terrain_, time, random_);
}

void World::CallAdds(uint32_t id, std::size_t phase) {
  const Monster& boss = monsters_.at(id);
  if (boss.motion == Motion::kDead) {
    return;
  }
  for (const monsters::Add& add : boss.phases[phase].adds()) {
    const auto kind = content_->monsters.kinds.find(add.kind());
    if (kind == content_->monsters.kinds.end()) {
      continue;
    }
    // Round the boss, evenly spaced, or where it stands, over the sea.
    for (uint32_t i = 0; i < add.count(); ++i) {
      const float angle = 2.0f * std::numbers::pi_v<float> *
                          static_cast<float>(i) /
                          static_cast<float>(add.count());
      float x = boss.x + kAddsAround * std::sin(angle);
      float z = boss.z + kAddsAround * std::cos(angle);
      if (!terrain_.Walkable(x, z)) {
        x = boss.x;
        z = boss.z;
      }
      // It fights where its boss does, for whoever it sees there.
      Monster called = MonsterOf(kind->second, x, z);
      called.called_by = id;
      called.sight = boss.sight;
      called.leash = boss.leash;
      Spawn(std::move(called));
    }
  }
}

void World::Announce(const Monster& boss, std::string_view news) {
  const auto kind = content_->monsters.kinds.find(boss.kind);
  if (kind == content_->monsters.kinds.end()) {
    return;
  }
  const std::string line = absl::StrCat("The ", kind->second.name(), " ", news);
  for (const auto& [id, player] : players_) {
    notices_.push_back(
        {.player = id, .text = line, .channel = protocol::CHANNEL_LAND});
  }
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
  Hit(found->first, player, monster, monster.damage);
}

void World::Hit(uint32_t id, Player& player, const Monster& monster, int blow) {
  // What the player wears takes the blow, down to one.
  const int damage = std::max(blow - player.stats.armour, 1);
  player.health = std::max(player.health - damage, 0);
  Event& hurt = Record(Event::Kind::kHurt, id, player);
  hurt.monster = monster.kind;
  hurt.amount = static_cast<uint32_t>(damage);
  if (player.health > 0) {
    return;
  }
  player.motion = Motion::kDead;
  player.respawn = kRespawnTime;
  player.way.clear();
  Record(Event::Kind::kDeath, id, player).monster = monster.kind;
}

void World::LandSkill(const Monster& monster) {
  const Place aimed{.x = monster.cast_x, .z = monster.cast_z};
  for (auto& [id, player] : players_) {
    if (player.motion != Motion::kDead &&
        InShape(monster.casting, aimed, monster.yaw,
                {.x = player.x, .z = player.z})) {
      Hit(id, player, monster, static_cast<int>(monster.casting.damage()));
    }
  }
}

}  // namespace nanommo
