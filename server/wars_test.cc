#include "server/wars.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include "content/skills.h"
#include "gtest/gtest.h"
#include "server/test_content.h"
#include "server/world.h"
#include "terrain/terrain.h"

namespace {

constexpr std::chrono::milliseconds kTick(50);

void Wait(nanommo::World& world, std::chrono::milliseconds time) {
  for (std::chrono::milliseconds run{0}; run < time; run += kTick) {
    world.Step(kTick);
  }
}

// A sword, and a whirl of it that hits all around, as Whirlwind does.
std::shared_ptr<const nanommo::Content> Swords() {
  return nanommo::testing::With(
      nanommo::testing::Sword(), *nanommo::ParseSkills(R"pb(
        skills {
          id: 1
          name: "Whirlwind"
          level: 1
          effect { cast_ms: 1000 hit_ms: 500 reach: 2.5 damage: 25 all: true }
        }
      )pb"));
}

// War 7, between guilds 1 and 2.
nanommo::War War() { return {.id = 7, .challenger = 1, .challenged = 2}; }

// Alice, player 1, swings her sword at bob, player 2, a metre away, and 0.65
// seconds in, he runs off. Her blow comes at 0.9 seconds, as he's 1.78
// metres from her, past the sword's 1.6. Three ticks before, 150
// milliseconds, he was 1.47 metres away.
void SwingAsBobRunsOff(nanommo::World& world) {
  world.Pick(1, 0, 2);
  world.SetInput(1, 0.0f, 0.0f, false, true);
  world.Step(kTick);
  world.SetInput(1, 0.0f, 0.0f, false);
  Wait(world, std::chrono::milliseconds(600));
  world.SetInput(2, 1.0f, 0.0f, true);
  Wait(world, std::chrono::milliseconds(300));
}

}  // namespace

TEST(Wars, MembersOfGuildsAtWarHitEachOther) {
  nanommo::World world(1, nanommo::Terrain(), Swords());
  world.Join(1, {.name = "alice", .guild = 1});
  world.Join(2, {.name = "bob", .guild = 2, .x = 1.0f});

  // Until their guilds are at war, neither can pick the other.
  world.Pick(1, 0, 2);
  EXPECT_EQ(world.players().at(1).target_player, 0u);
  world.SetWars({War()});
  EXPECT_TRUE(world.Hostile(1, 2));
  EXPECT_TRUE(world.Hostile(2, 1));

  // Each picks the other, as they would a monster, and swings a sword, for
  // fifteen, 0.9 seconds in.
  world.Pick(1, 0, 2);
  world.Pick(2, 0, 1);
  EXPECT_EQ(world.players().at(1).target_player, 2u);
  world.SetInput(1, 0.0f, 0.0f, false, true);
  world.SetInput(2, 0.0f, 0.0f, false, true);
  Wait(world, std::chrono::seconds(1));
  EXPECT_EQ(world.players().at(1).health, 100 - 15);
  EXPECT_EQ(world.players().at(2).health, 100 - 15);

  // Once the war's over, neither is the other's to pick, and their next
  // swings go at the air.
  world.SetWars({});
  EXPECT_FALSE(world.Hostile(1, 2));
  world.Pick(1, 0, 2);
  world.Pick(2, 0, 1);
  EXPECT_EQ(world.players().at(1).target_player, 0u);
  Wait(world, std::chrono::seconds(3));
  EXPECT_EQ(world.players().at(1).health, 100 - 15);
  EXPECT_EQ(world.players().at(2).health, 100 - 15);
}

TEST(Wars, NobodyElseCanHurtAPlayer) {
  nanommo::World world(1, nanommo::Terrain(), Swords());
  world.Join(1, {.name = "alice", .guild = 1, .skills = {1}});
  world.Join(2, {.name = "bob", .guild = 2, .x = 1.0f});
  world.Join(3, {.name = "carol", .guild = 1, .x = -1.0f});
  world.Join(4, {.name = "dave", .guild = 3, .z = 1.0f, .skills = {1}});
  world.SetWars({War()});

  // Carol picks alice, of her own guild, and dave picks her, whose guild
  // isn't at war with his: neither is picked. Bob, her enemy, picks her.
  world.Pick(3, 0, 1);
  world.Pick(4, 0, 1);
  world.Pick(2, 0, 1);
  EXPECT_EQ(world.players().at(3).target_player, 0u);
  EXPECT_EQ(world.players().at(4).target_player, 0u);
  EXPECT_EQ(world.players().at(2).target_player, 1u);

  // Alice's whirl hits all around her: bob, and not carol or dave, beside
  // her too. Dave's hits none of them.
  world.UseSkill(1, 1);
  world.UseSkill(4, 1);
  Wait(world, std::chrono::seconds(1));
  EXPECT_EQ(world.players().at(2).health, 100 - 25);
  EXPECT_EQ(world.players().at(1).health, 100);
  EXPECT_EQ(world.players().at(3).health, 100);
  EXPECT_EQ(world.players().at(4).health, 100);
}

TEST(Wars, AnEnemysBlowKillsAndCountsForTheirSide) {
  nanommo::World world(1, nanommo::Terrain(), Swords());
  world.Join(1, {.name = "alice", .guild = 1});
  world.Join(2, {.name = "bob", .guild = 2, .x = 1.0f, .health = 10});
  world.SetWars({War()});
  world.TakeEvents();

  // Alice's fifteen leaves bob nothing, and he's dead, as a monster would
  // have him, until he comes back.
  world.Pick(1, 0, 2);
  world.SetInput(1, 0.0f, 0.0f, false, true);
  Wait(world, std::chrono::seconds(1));
  world.SetInput(1, 0.0f, 0.0f, false);
  const nanommo::Player& bob = world.players().at(2);
  EXPECT_EQ(bob.health, 0);
  EXPECT_EQ(bob.motion, nanommo::Motion::kDead);
  EXPECT_GT(bob.respawn, std::chrono::seconds(9));
  int deaths = 0;
  for (const nanommo::Event& event : world.TakeEvents()) {
    deaths += event.kind == nanommo::Event::Kind::kDeath ? 1 : 0;
  }
  EXPECT_EQ(deaths, 1);

  // The kill is handed over once, for the war's score, and gives alice
  // nothing: no experience, and nothing in her bag.
  const std::vector<nanommo::WarKill> kills = world.TakeWarKills();
  ASSERT_EQ(kills.size(), 1u);
  EXPECT_EQ(kills[0].war, 7u);
  EXPECT_EQ(kills[0].killer, 1u);
  EXPECT_EQ(kills[0].victim, 2u);
  EXPECT_EQ(kills[0].guild, 1u);
  EXPECT_TRUE(world.TakeWarKills().empty());
  EXPECT_EQ(world.players().at(1).experience, 0u);
  EXPECT_TRUE(world.players().at(1).inventory.items().bag().empty());

  // Once his wait's over, he comes back at full health.
  Wait(world, nanommo::World::kRespawnTime);
  world.Respawn(2);
  EXPECT_EQ(bob.motion, nanommo::Motion::kIdle);
  EXPECT_EQ(bob.health, 100);
}

TEST(Wars, ADuellistAnEnemyKillsLosesTheDuel) {
  nanommo::World world(1, nanommo::Terrain(), Swords());
  world.Join(1, {.name = "alice", .guild = 1, .health = 10});
  world.Join(2, {.name = "bob", .guild = 2, .x = 1.0f});
  world.Join(3, {.name = "carol", .x = -1.0f});
  world.SetWars({War()});
  ASSERT_TRUE(world.AskToDuel(1, "carol").ok());
  ASSERT_TRUE(world.AnswerDuel(3, true).ok());
  Wait(world, nanommo::Duels::kCountdown);
  ASSERT_EQ(world.DuelOf(1).state(), nanommo::protocol::DUEL_FIGHTING);
  world.TakeNotices();

  // Bob isn't her opponent, but he's her enemy, and his blow kills her, which
  // ends the duel, as a monster's would.
  world.Pick(2, 0, 1);
  world.SetInput(2, 0.0f, 0.0f, false, true);
  Wait(world, std::chrono::seconds(1));
  EXPECT_EQ(world.players().at(1).motion, nanommo::Motion::kDead);
  EXPECT_EQ(world.DuelOf(3).state(), nanommo::protocol::DUEL_NONE);
  const std::vector<nanommo::Notice> notices = world.TakeNotices();
  ASSERT_EQ(notices.size(), 2u);
  EXPECT_EQ(notices[0].player, 1u);
  EXPECT_EQ(notices[0].text, "carol won the duel.");
  EXPECT_EQ(notices[1].player, 3u);
  EXPECT_EQ(notices[1].text, "You won the duel against alice.");
  const std::vector<nanommo::WarKill> kills = world.TakeWarKills();
  ASSERT_EQ(kills.size(), 1u);
  EXPECT_EQ(kills[0].killer, 2u);
  EXPECT_EQ(kills[0].guild, 2u);
}

TEST(Wars, ABlowOnAnEnemyIsCheckedWhereTheAttackerSawThem) {
  const auto bob_after = [](std::chrono::milliseconds behind) {
    nanommo::World world(1, nanommo::Terrain(), Swords());
    world.Join(1, {.name = "alice", .guild = 1});
    world.Join(2, {.name = "bob", .guild = 2, .x = 1.0f});
    world.SetWars({War()});
    world.SetBehind(1, behind);
    SwingAsBobRunsOff(world);
    return world.players().at(2);
  };

  // Checked where bob is as her blow lands, it misses. Her inputs say her
  // client draws everyone 150 milliseconds behind, when he was in reach, and
  // it lands, where he's run to.
  EXPECT_EQ(bob_after(std::chrono::milliseconds(0)).health, 100);
  const nanommo::Player bob = bob_after(std::chrono::milliseconds(150));
  EXPECT_EQ(bob.health, 100 - 15);
  EXPECT_GT(bob.x, 1.6f);
}
