#include <chrono>
#include <cstdint>

#include "gtest/gtest.h"
#include "server/monsters.h"
#include "server/test_content.h"
#include "server/world.h"
#include "terrain/terrain.h"

namespace {

constexpr std::chrono::milliseconds kTick(50);

void RunOneSecond(nanommo::World& world) {
  for (int tick = 0; tick < 20; ++tick) {
    world.Step(kTick);
  }
}

// The tests' boss: a hundred health, a slam all round it and a sweep in front
// of it, each from a metre and a half, in three phases, from full health, from
// 60 and from 30: the slam alone, then the sweep alone, and then both.
nanommo::MonsterTable Monsters() {
  return *nanommo::ParseMonsters(R"pb(
    kinds {
      id: 1
      name: "Colossus"
      level: 10
      max_health: 100
      experience: 500
      fight {
        damage: 5
        swing_ms: 900
        hit_ms: 650
        reach: 1.2
        sight: 3
        leash: 5
        wander: 1
      }
      skills {
        id: 1
        name: "Slam"
        size: 3
        telegraph_ms: 2000
        damage: 30
        cooldown_ms: 10000
        range: 1.5
      }
      skills {
        id: 2
        name: "Sweep"
        shape: CONE
        size: 3
        arc: 90
        telegraph_ms: 2000
        damage: 30
        cooldown_ms: 10000
        range: 1.5
      }
      phases { health: 100 skills: 1 }
      phases { health: 60 skills: 2 }
      phases {
        health: 30
        skills: [ 1, 2 ]
      }
    }
  )pb");
}

nanommo::monsters::Kind Colossus() { return Monsters().kinds.at(1); }

// A blow of ten at the monster the player picked, from up to ten metres,
// which they can use again as soon as it's over.
nanommo::SkillTable Smite() {
  return *nanommo::ParseSkills(R"pb(
    skills {
      id: 1
      name: "Smite"
      level: 1
      effect { cast_ms: 100 hit_ms: 50 reach: 10 damage: 10 }
    }
  )pb");
}

// The player smites the monster they picked, so many times over.
void Smite(nanommo::World& world, uint32_t player, int times = 1) {
  for (int blow = 0; blow < times; ++blow) {
    world.UseSkill(player, 1);
    for (int tick = 0; tick < 3; ++tick) {
      world.Step(kTick);
    }
  }
}

}  // namespace

TEST(World, TheBossChangesPhaseAtItsHealth) {
  nanommo::World world(
      1, nanommo::Terrain(),
      nanommo::testing::With(nanommo::testing::Sword(), Smite()));
  world.Join(1, {.x = 4.0f, .skills = {1}});
  const uint32_t id = world.Spawn(nanommo::MonsterOf(Colossus(), 0.0f, 0.0f));
  const nanommo::Monster& colossus = world.monsters().at(id);
  world.Pick(1, id);

  // At 70, it's still in its first phase, and at 60, it's in its second.
  Smite(world, 1, 3);
  EXPECT_EQ(colossus.health, 70);
  EXPECT_EQ(colossus.phase, 0u);
  Smite(world, 1);
  EXPECT_EQ(colossus.phase, 1u);

  // There it sweeps, as the second phase has it, though its slam's ready.
  for (int tick = 0; tick < 40 && colossus.motion != nanommo::Motion::kSkill;
       ++tick) {
    world.Step(kTick);
  }
  ASSERT_EQ(colossus.motion, nanommo::Motion::kSkill);
  EXPECT_EQ(colossus.casting.id(), 2u);
  EXPECT_FALSE(colossus.cooldowns.contains(1));

  // At 30, it's in its third, and once it's dead, it comes back in its
  // first.
  Smite(world, 1, 3);
  EXPECT_EQ(colossus.phase, 2u);
  Smite(world, 1, 3);
  ASSERT_EQ(colossus.motion, nanommo::Motion::kDead);
  for (int second = 0; second < nanommo::World::kRespawnTime.count();
       ++second) {
    RunOneSecond(world);
  }
  EXPECT_NE(colossus.motion, nanommo::Motion::kDead);
  EXPECT_EQ(colossus.health, 100);
  EXPECT_EQ(colossus.phase, 0u);
}
