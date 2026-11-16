#include "server/shown.h"

#include <chrono>
#include <cstdint>
#include <optional>

#include "server/levels.h"

namespace nanommo {

protocol::Motion ToProtocol(Motion motion) {
  switch (motion) {
    case Motion::kWalk:
      return protocol::MOTION_WALK;
    case Motion::kRun:
      return protocol::MOTION_RUN;
    case Motion::kAttack:
      return protocol::MOTION_ATTACK;
    case Motion::kDead:
      return protocol::MOTION_DEAD;
    case Motion::kSkill:
      return protocol::MOTION_SKILL;
    case Motion::kIdle:
      break;
  }
  return protocol::MOTION_IDLE;
}

protocol::Player ToProtocol(uint32_t id, const Player& player) {
  protocol::Player sent;
  sent.set_id(id);
  sent.set_x(player.x);
  sent.set_z(player.z);
  sent.set_name(player.name);
  sent.set_yaw(player.yaw);
  sent.set_motion(ToProtocol(player.motion));
  const int level = LevelFor(player.experience).number;
  sent.set_level(level);
  sent.set_health(player.health);
  sent.set_max_health(MaxHealthOf(player));
  sent.set_weapon(MainHandOf(player));
  for (const uint32_t item : WornOf(player)) {
    sent.add_wearing(item);
  }
  if (player.motion == Motion::kSkill) {
    sent.set_skill(player.skill);
    sent.set_target(player.target);
    sent.set_target_player(player.target_player);
    sent.set_casts(player.casts);
  }
  return sent;
}

protocol::Monster ToProtocol(uint32_t id, const Monster& monster) {
  protocol::Monster sent;
  sent.set_id(id);
  sent.set_kind(monster.kind);
  sent.set_x(monster.x);
  sent.set_z(monster.z);
  sent.set_yaw(monster.yaw);
  sent.set_motion(ToProtocol(monster.motion));
  sent.set_level(monster.level);
  sent.set_health(monster.health);
  sent.set_max_health(monster.max_health);
  sent.set_height(monster.height);
  if (monster.boss()) {
    sent.set_phase(static_cast<uint32_t>(monster.phase) + 1);
  }
  if (monster.motion == Motion::kSkill) {
    sent.set_skill(monster.casting.id());
    sent.set_lands_in(static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::milliseconds(monster.casting.telegraph_ms()) -
            monster.cast)
            .count()));
  }
  return sent;
}

protocol::Npc ToProtocol(uint32_t id, const Npc& npc) {
  protocol::Npc sent;
  sent.set_id(id);
  sent.set_kind(npc.kind);
  sent.set_x(npc.x);
  sent.set_z(npc.z);
  sent.set_yaw(npc.yaw);
  return sent;
}

protocol::World YouIn(uint32_t id, const Player& player, uint32_t seed,
                      int land, std::optional<uint32_t> acked) {
  const Level level = LevelFor(player.experience);
  protocol::World you;
  you.set_you(id);
  you.set_experience(level.experience);
  you.set_next_level(level.next);
  for (const auto& [skill, left] : player.cooldowns) {
    (*you.mutable_cooldowns())[skill] = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(left).count());
  }
  you.mutable_skills()->Add(player.skills.begin(), player.skills.end());
  if (player.motion == Motion::kDead) {
    you.set_respawn(static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(player.respawn)
            .count()));
  }
  you.set_seed(seed);
  you.set_land(static_cast<uint32_t>(land));
  if (acked) {
    you.set_acked(*acked);
  }
  you.set_steered(player.steered);
  // Where you are, with the last input applied, in every datagram, so the
  // two always come together, whichever part has you otherwise.
  protocol::Player* here = you.add_players();
  *here = ToProtocol(id, player);
  here->clear_name();
  here->clear_level();
  here->clear_max_health();
  here->clear_weapon();
  return you;
}

protocol::World BagOf(const Player& player) {
  protocol::World bag;
  *bag.mutable_inventory() = player.inventory.items();
  return bag;
}

protocol::World QuestsOf(const Player& player) {
  protocol::World quests;
  protocol::Quests* on = quests.mutable_quests();
  on->mutable_on()->insert(player.quests.begin(), player.quests.end());
  on->mutable_done()->Add(player.quests_done.begin(), player.quests_done.end());
  on->set_revision(player.quests_revision);
  return quests;
}

std::optional<protocol::Event> ForPlayers(const Event& event) {
  protocol::Event shown;
  shown.set_player(event.player_id);
  shown.set_amount(event.amount);
  switch (event.kind) {
    case Event::Kind::kHit:
      shown.set_kind(protocol::EVENT_HIT);
      shown.set_monster(event.monster_id);
      return shown;
    case Event::Kind::kHurt:
      shown.set_kind(protocol::EVENT_HURT);
      return shown;
    case Event::Kind::kHeal:
      shown.set_kind(protocol::EVENT_HEAL);
      return shown;
    case Event::Kind::kLevel:
      shown.set_kind(protocol::EVENT_LEVEL);
      shown.set_amount(event.level);
      return shown;
    case Event::Kind::kKill:
    case Event::Kind::kDeath:
    case Event::Kind::kLoot:
    case Event::Kind::kPrize:
    case Event::Kind::kTake:
    case Event::Kind::kTurnIn:
    case Event::Kind::kUse:
      return std::nullopt;
  }
  return std::nullopt;
}

}  // namespace nanommo
