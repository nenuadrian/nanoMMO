#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <string>
#include <vector>

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
// 60 and from 30: the slam alone, then the sweep alone, calling up two
// skeletons, and then both, calling up three more. It always drops two bones.
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
      loot { item: 3 chance: 1 min: 2 max: 2 }
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
      phases {
        health: 60
        skills: 2
        adds { kind: 2 count: 2 }
      }
      phases {
        health: 30
        skills: [ 1, 2 ]
        adds { kind: 2 count: 3 }
      }
    }
    kinds {
      id: 2
      name: "Skeleton"
      level: 1
      max_health: 50
      experience: 50
      fight {
        damage: 5
        swing_ms: 900
        hit_ms: 650
        reach: 1.2
        sight: 3
        leash: 4
        wander: 3
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

// How many adds the boss has called up, alive or dead.
std::ptrdiff_t AddsOf(const nanommo::World& world, uint32_t boss) {
  return std::ranges::count(world.monsters() | std::views::values, boss,
                            &nanommo::Monster::called_by);
}

// The lines the player's heard since the last time, among everyone's.
std::vector<std::string> Heard(nanommo::World& world, uint32_t player) {
  std::vector<std::string> heard;
  for (const nanommo::Notice& notice : world.TakeNotices()) {
    if (notice.player == player) {
      heard.push_back(notice.text);
    }
  }
  return heard;
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

TEST(World, TheBossFightsWhoHurtItMost) {
  nanommo::World world(
      1, nanommo::Terrain(),
      nanommo::testing::With(nanommo::testing::Sword(), Smite()));
  // Either side of it, out of its sight, and within its leash.
  world.Join(1, {.x = 4.5f, .skills = {1}});
  world.Join(2, {.x = -4.5f, .skills = {1}});
  const uint32_t id = world.Spawn(nanommo::MonsterOf(Colossus(), 0.0f, 0.0f));
  const nanommo::Monster& colossus = world.monsters().at(id);
  world.Pick(1, id);
  world.Pick(2, id);

  // The second hurt it last, and the first most, so it goes for the first.
  Smite(world, 1, 2);
  Smite(world, 2);
  EXPECT_EQ(colossus.prey, 1u);
  EXPECT_GT(colossus.x, 0.0f);

  // Until the second has hurt it more.
  Smite(world, 2, 2);
  EXPECT_EQ(colossus.threat.at(1), 20);
  EXPECT_EQ(colossus.threat.at(2), 30);
  EXPECT_EQ(colossus.prey, 2u);
}

TEST(World, TheBossGoesHomeWhenNobodysLeft) {
  nanommo::World world(
      1, nanommo::Terrain(),
      nanommo::testing::With(nanommo::testing::Sword(), Smite()));
  world.Join(1, {.x = 4.5f, .skills = {1}});
  const uint32_t id = world.Spawn(nanommo::MonsterOf(Colossus(), 0.0f, 0.0f));
  const nanommo::Monster& colossus = world.monsters().at(id);
  world.Pick(1, id);

  // Hurt into its second phase, it comes for them.
  Smite(world, 1, 5);
  EXPECT_EQ(colossus.phase, 1u);
  EXPECT_GT(colossus.x, 0.0f);

  // They run past its leash, and it goes home, back at full health, in its
  // first phase, and it's forgotten them.
  world.SetInput(1, 1.0f, 0.0f, true);
  for (int tick = 0; tick < 100 && !colossus.evading; ++tick) {
    world.Step(kTick);
  }
  ASSERT_TRUE(colossus.evading);
  EXPECT_EQ(colossus.health, 100);
  EXPECT_EQ(colossus.phase, 0u);
  EXPECT_TRUE(colossus.threat.empty());

  // Nothing hurts it on its way.
  Smite(world, 1);
  EXPECT_EQ(colossus.health, 100);

  // Once it's home, it stays there, as they're still past its leash.
  for (int tick = 0; tick < 100 && colossus.evading; ++tick) {
    world.Step(kTick);
  }
  EXPECT_FALSE(colossus.evading);
  EXPECT_EQ(colossus.x, 0.0f);
  EXPECT_EQ(colossus.z, 0.0f);
  RunOneSecond(world);
  EXPECT_EQ(colossus.prey, 0u);
  EXPECT_EQ(colossus.x, 0.0f);
}

TEST(World, EachPhaseBringsItsAdds) {
  nanommo::World world(
      1, nanommo::Terrain(),
      nanommo::testing::With(nanommo::testing::Sword(), Smite(), Monsters()));
  world.Join(1, {.x = 4.5f, .skills = {1}});
  const uint32_t id = world.Spawn(nanommo::MonsterOf(Colossus(), 0.0f, 0.0f));
  const nanommo::Monster& colossus = world.monsters().at(id);
  world.Pick(1, id);

  // Into its second phase, it calls up two skeletons round it.
  Smite(world, 1, 3);
  EXPECT_EQ(AddsOf(world, id), 0);
  Smite(world, 1);
  ASSERT_EQ(AddsOf(world, id), 2);
  for (const auto& [add_id, add] : world.monsters()) {
    if (add.called_by == id) {
      EXPECT_EQ(add.kind, 2u);
      EXPECT_LT(std::hypot(add.x - colossus.x, add.z - colossus.z), 4.0f);
    }
  }

  // Into its third, three more.
  Smite(world, 1, 3);
  EXPECT_EQ(colossus.phase, 2u);
  EXPECT_EQ(AddsOf(world, id), 5);
}

TEST(World, TheBossesAddsGoWhenItGoesHome) {
  nanommo::World world(
      1, nanommo::Terrain(),
      nanommo::testing::With(nanommo::testing::Sword(), Smite(), Monsters()));
  world.Join(1, {.x = 4.5f, .skills = {1}});
  const uint32_t id = world.Spawn(nanommo::MonsterOf(Colossus(), 0.0f, 0.0f));
  const nanommo::Monster& colossus = world.monsters().at(id);
  world.Pick(1, id);
  Smite(world, 1, 7);
  ASSERT_EQ(AddsOf(world, id), 5);

  // Its player runs past its leash, and it goes home without them.
  world.SetInput(1, 1.0f, 0.0f, true);
  for (int tick = 0; tick < 100 && !colossus.evading; ++tick) {
    world.Step(kTick);
  }
  ASSERT_TRUE(colossus.evading);
  EXPECT_EQ(AddsOf(world, id), 0);
  EXPECT_EQ(world.monsters().size(), 1u);
}

TEST(World, TheBossesAddsFallWithItAndDontComeBack) {
  nanommo::World world(
      1, nanommo::Terrain(),
      nanommo::testing::With(nanommo::testing::Sword(), Smite(), Monsters()));
  world.Join(1, {.x = 4.5f, .skills = {1}});
  const uint32_t id = world.Spawn(nanommo::MonsterOf(Colossus(), 0.0f, 0.0f));
  const nanommo::Monster& colossus = world.monsters().at(id);
  world.Pick(1, id);

  // They fall as it dies, and lie there while it does.
  Smite(world, 1, 10);
  ASSERT_EQ(colossus.motion, nanommo::Motion::kDead);
  ASSERT_EQ(AddsOf(world, id), 5);
  for (const auto& [add_id, add] : world.monsters()) {
    EXPECT_EQ(add.motion, nanommo::Motion::kDead);
  }

  // They're gone once it's back, which starts without them.
  for (int second = 0; second < nanommo::World::kRespawnTime.count();
       ++second) {
    RunOneSecond(world);
  }
  EXPECT_NE(colossus.motion, nanommo::Motion::kDead);
  EXPECT_EQ(AddsOf(world, id), 0);
}

TEST(World, EveryoneWhoHelpedGetsALootRoll) {
  nanommo::World world(
      1, nanommo::Terrain(),
      nanommo::testing::With(nanommo::testing::Gear(), Smite(), Monsters()));
  world.Join(1, {.name = "alice", .x = 4.5f, .skills = {1}});
  world.Join(2, {.name = "bob", .x = -4.5f, .skills = {1}});
  world.Join(3, {.name = "carol", .z = 4.5f, .skills = {1}});
  // Near it, and not hitting it.
  world.Join(4, {.name = "dave", .z = -4.5f});
  const uint32_t id = world.Spawn(nanommo::MonsterOf(Colossus(), 0.0f, 0.0f));
  for (const uint32_t player : {1u, 2u, 3u}) {
    world.Pick(player, id);
  }

  // Carol lands the last blow, and alice took the most.
  Smite(world, 1, 4);
  Smite(world, 2, 3);
  Smite(world, 3, 3);
  ASSERT_EQ(world.monsters().at(id).motion, nanommo::Motion::kDead);

  // Each of them gets its experience, and two bones, as each rolls its loot.
  for (const uint32_t player : {1u, 2u, 3u}) {
    const nanommo::Player& helper = world.players().at(player);
    EXPECT_EQ(helper.experience, 500u);
    const nanommo::items::Stack* bones = helper.inventory.At(nanommo::InBag(1));
    ASSERT_NE(bones, nullptr);
    EXPECT_EQ(bones->item(), 3u);
    EXPECT_EQ(bones->count(), 2u);
  }
  EXPECT_EQ(world.players().at(4).experience, 0u);
  EXPECT_EQ(world.players().at(4).inventory.At(nanommo::InBag(1)), nullptr);
}

TEST(World, ALittleHelpIsntEnough) {
  nanommo::World world(
      1, nanommo::Terrain(),
      nanommo::testing::With(nanommo::testing::Gear(), Smite(), Monsters()));
  // At level 19, a smite takes a hundred.
  world.Join(1,
             {.name = "alice", .x = 4.5f, .experience = 17100, .skills = {1}});
  world.Join(2, {.name = "bob", .x = -4.5f, .skills = {1}});
  world.Join(3, {.name = "carol", .z = 4.5f, .skills = {1}});
  nanommo::Monster tougher = nanommo::MonsterOf(Colossus(), 0.0f, 0.0f);
  tougher.max_health = 1000;
  const uint32_t id = world.Spawn(tougher);
  const nanommo::Monster& colossus = world.monsters().at(id);
  for (const uint32_t player : {1u, 2u, 3u}) {
    world.Pick(player, id);
  }

  // Bob takes 4% of its health, carol 5%, and alice the rest.
  Smite(world, 2, 4);
  Smite(world, 3, 5);
  Smite(world, 1, 10);
  ASSERT_EQ(colossus.motion, nanommo::Motion::kDead);
  EXPECT_EQ(colossus.threat.at(1), 910);
  EXPECT_EQ(colossus.threat.at(2), 40);
  EXPECT_EQ(colossus.threat.at(3), 50);

  // Bob gets nothing, and carol as much as alice.
  EXPECT_EQ(world.players().at(2).experience, 0u);
  EXPECT_EQ(world.players().at(2).inventory.At(nanommo::InBag(1)), nullptr);
  EXPECT_EQ(world.players().at(3).experience, 500u);
  EXPECT_NE(world.players().at(3).inventory.At(nanommo::InBag(1)), nullptr);
  EXPECT_EQ(world.players().at(1).experience, 17100u + 500u);
}

TEST(World, TheLandHearsTheBossFallAndRiseAgain) {
  nanommo::World world(
      1, nanommo::Terrain(),
      nanommo::testing::With(nanommo::testing::Gear(), Smite(), Monsters()));
  world.Join(1,
             {.name = "alice", .x = 4.5f, .experience = 17100, .skills = {1}});
  world.Join(2, {.name = "bob", .x = -4.5f});
  nanommo::Monster colossus = nanommo::MonsterOf(Colossus(), 0.0f, 0.0f);
  colossus.respawn_time = std::chrono::seconds(40);
  const uint32_t id = world.Spawn(colossus);
  world.Pick(1, id);

  // Alice takes it all in one blow, and everyone hears.
  Smite(world, 1);
  ASSERT_EQ(world.monsters().at(id).motion, nanommo::Motion::kDead);
  EXPECT_EQ(Heard(world, 2),
            std::vector<std::string>{"The Colossus has fallen to alice. It "
                                     "rises again in 40 seconds."});

  // Thirty seconds before it's back, and as it is.
  for (int second = 0; second < 10; ++second) {
    RunOneSecond(world);
  }
  EXPECT_EQ(Heard(world, 2), std::vector<std::string>{
                                 "The Colossus rises again in 30 seconds."});
  for (int second = 0; second < 30; ++second) {
    RunOneSecond(world);
  }
  EXPECT_NE(world.monsters().at(id).motion, nanommo::Motion::kDead);
  EXPECT_EQ(Heard(world, 2),
            std::vector<std::string>{"The Colossus has risen."});
}
