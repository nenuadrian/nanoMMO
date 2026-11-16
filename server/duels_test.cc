#include "server/duels.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <set>
#include <vector>

#include "content/skills.h"
#include "gtest/gtest.h"
#include "server/test_content.h"
#include "server/world.h"
#include "terrain/terrain.h"

namespace {

constexpr std::chrono::milliseconds kTick(50);

// An island, as a duel reaches further than the tests' board. Its players
// start in a clearing, sixteen metres around the middle, where no tree grows.
nanommo::World Island() {
  return nanommo::World(1, nanommo::Terrain(1),
                        nanommo::testing::With(nanommo::testing::Sword()));
}

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

// Alice, player 1, asks bob, player 2, and he says yes, and the countdown
// runs out.
void StartDuel(nanommo::World& world) {
  ASSERT_TRUE(world.AskToDuel(1, "bob").ok());
  ASSERT_TRUE(world.AnswerDuel(2, true).ok());
  Wait(world, nanommo::Duels::kCountdown);
  ASSERT_EQ(world.DuelOf(1).state(), nanommo::protocol::DUEL_FIGHTING);
  world.TakeNotices();
}

// Alice swings her sword at bob, a metre away, and 0.65 seconds in, he runs
// off. Her blow comes at 0.9 seconds, as he's 1.78 metres from her, past the
// sword's 1.6. Three ticks before, 150 milliseconds, he was 1.47 metres away.
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

TEST(Duels, AChallengeWaitsForAYes) {
  nanommo::World world = Island();
  world.Join(1, {.name = "alice", .x = -2.0f});
  world.Join(2, {.name = "bob", .x = 3.0f});

  // By their name, whatever its case.
  ASSERT_TRUE(world.AskToDuel(1, "BOB").ok());
  const std::vector<nanommo::Notice> notices = world.TakeNotices();
  ASSERT_EQ(notices.size(), 2u);
  EXPECT_EQ(notices[0].player, 2u);
  EXPECT_EQ(notices[0].text,
            "alice asks you to duel. Type /duel yes to say yes.");
  EXPECT_EQ(notices[1].text, "You asked bob to duel.");
  EXPECT_EQ(world.DuelOf(1).state(), nanommo::protocol::DUEL_ASKING);
  EXPECT_EQ(world.DuelOf(2).state(), nanommo::protocol::DUEL_ASKED);
  EXPECT_EQ(world.DuelOf(2).opponent(), "alice");
  EXPECT_EQ(world.TakeDuelNews(), (std::set<uint32_t>{1, 2}));

  // Nothing starts until bob says yes, and alice can't say it for him.
  Wait(world, std::chrono::seconds(10));
  EXPECT_EQ(world.DuelOf(2).state(), nanommo::protocol::DUEL_ASKED);
  EXPECT_EQ(world.AnswerDuel(1, true).message(), "Nobody's asked you to duel.");
  ASSERT_TRUE(world.AnswerDuel(2, true).ok());

  // The flag goes down where alice stands, and the countdown starts.
  nanommo::protocol::Duel duel = world.DuelOf(2);
  EXPECT_EQ(duel.state(), nanommo::protocol::DUEL_COUNTDOWN);
  EXPECT_EQ(duel.countdown(), 3000u);
  EXPECT_FLOAT_EQ(duel.flag().x(), -2.0f);
  EXPECT_EQ(world.TakeDuelNews(), (std::set<uint32_t>{1, 2}));

  // Both hear as each second goes, and it starts three seconds on.
  Wait(world, std::chrono::seconds(1));
  EXPECT_EQ(world.DuelOf(1).countdown(), 2000u);
  EXPECT_EQ(world.TakeDuelNews(), (std::set<uint32_t>{1, 2}));
  Wait(world, std::chrono::milliseconds(1950));
  EXPECT_EQ(world.DuelOf(1).state(), nanommo::protocol::DUEL_COUNTDOWN);
  world.Step(kTick);
  duel = world.DuelOf(1);
  EXPECT_EQ(duel.state(), nanommo::protocol::DUEL_FIGHTING);
  EXPECT_EQ(duel.opponent_id(), 2u);
}

TEST(Duels, TooFarToChallenge) {
  nanommo::World world = Island();
  world.Join(1, {.name = "alice"});
  world.Join(2, {.name = "bob", .x = 10.5f});
  EXPECT_EQ(world.AskToDuel(1, "bob").message(),
            "You're too far away to duel bob.");
  EXPECT_EQ(world.DuelOf(2).state(), nanommo::protocol::DUEL_NONE);
  EXPECT_TRUE(world.TakeNotices().empty());
}

TEST(Duels, NearEnoughToChallenge) {
  nanommo::World world = Island();
  world.Join(1, {.name = "alice"});
  world.Join(2, {.name = "bob", .x = 10.0f});
  ASSERT_TRUE(world.AskToDuel(1, "bob").ok());
  EXPECT_EQ(world.DuelOf(2).state(), nanommo::protocol::DUEL_ASKED);
}

TEST(Duels, AnAskRunsOutAfterThirtySeconds) {
  nanommo::World world = Island();
  world.Join(1, {.name = "alice"});
  world.Join(2, {.name = "bob", .x = 3.0f});
  ASSERT_TRUE(world.AskToDuel(1, "bob").ok());
  world.TakeNotices();
  world.TakeDuelNews();

  Wait(world, std::chrono::milliseconds(29950));
  EXPECT_EQ(world.DuelOf(2).state(), nanommo::protocol::DUEL_ASKED);
  world.Step(kTick);
  EXPECT_EQ(world.DuelOf(1).state(), nanommo::protocol::DUEL_NONE);
  EXPECT_EQ(world.DuelOf(2).state(), nanommo::protocol::DUEL_NONE);
  EXPECT_EQ(world.TakeDuelNews(), (std::set<uint32_t>{1, 2}));
  const std::vector<nanommo::Notice> notices = world.TakeNotices();
  ASSERT_EQ(notices.size(), 1u);
  EXPECT_EQ(notices[0].player, 1u);
  EXPECT_EQ(notices[0].text, "bob didn't answer, so there's no duel.");
  EXPECT_EQ(world.AnswerDuel(2, true).message(), "Nobody's asked you to duel.");
}

TEST(Duels, LeavingTheFlagLosesTheDuel) {
  nanommo::World world = Island();
  world.Join(1, {.name = "alice"});
  world.Join(2, {.name = "bob", .x = 3.0f});
  ASSERT_TRUE(world.AskToDuel(1, "bob").ok());
  ASSERT_TRUE(world.AnswerDuel(2, true).ok());
  Wait(world, nanommo::Duels::kCountdown);
  ASSERT_EQ(world.DuelOf(2).state(), nanommo::protocol::DUEL_FIGHTING);
  world.TakeNotices();

  // Bob walks off, round any trees, and the duel's on until he's more than
  // 25 metres from the flag, where alice stood.
  const nanommo::Place away{.x = 40.0f, .z = 0.0f};
  ASSERT_TRUE(world.terrain().Walkable(away.x, away.z));
  world.WalkTo(2, away);
  const auto from_flag = [&world] {
    const nanommo::Player& bob = world.players().at(2);
    return std::hypot(bob.x, bob.z);
  };
  for (int tick = 0; from_flag() <= nanommo::Duels::kFlagReach; ++tick) {
    ASSERT_EQ(world.DuelOf(2).state(), nanommo::protocol::DUEL_FIGHTING);
    ASSERT_LT(tick, 400) << "bob didn't get far enough";
    world.Step(kTick);
  }
  EXPECT_EQ(world.DuelOf(1).state(), nanommo::protocol::DUEL_NONE);
  const std::vector<nanommo::Notice> notices = world.TakeNotices();
  ASSERT_EQ(notices.size(), 3u);
  EXPECT_EQ(notices[0].player, 2u);
  EXPECT_EQ(notices[0].text, "You went too far from the flag.");
  EXPECT_EQ(notices[1].text, "alice won the duel.");
  EXPECT_EQ(notices[2].player, 1u);
  EXPECT_EQ(notices[2].text, "You won the duel against bob.");
}

TEST(Duels, DuelistsHitEachOther) {
  nanommo::World world(1, nanommo::Terrain(), Swords());
  world.Join(1, {.name = "alice"});
  world.Join(2, {.name = "bob", .x = 1.0f});
  StartDuel(world);

  // Each picks the other, as they would a monster, and swings a sword, for
  // fifteen, 0.9 seconds in.
  world.Pick(1, 0, 2);
  world.Pick(2, 0, 1);
  world.SetInput(1, 0.0f, 0.0f, false, true);
  world.SetInput(2, 0.0f, 0.0f, false, true);
  world.TakeEvents();
  Wait(world, std::chrono::seconds(1));
  EXPECT_EQ(world.players().at(1).health, 100 - 15);
  EXPECT_EQ(world.players().at(2).health, 100 - 15);
  // Each blow is an event, for the players near it to see.
  int hurt = 0;
  for (const nanommo::Event& event : world.TakeEvents()) {
    if (event.kind == nanommo::Event::Kind::kHurt) {
      EXPECT_EQ(event.amount, 15u);
      ++hurt;
    }
  }
  EXPECT_EQ(hurt, 2);
  EXPECT_EQ(world.DuelOf(1).state(), nanommo::protocol::DUEL_FIGHTING);
}

TEST(Duels, OthersCantHitADuelist) {
  nanommo::World world(1, nanommo::Terrain(), Swords());
  world.Join(1, {.name = "alice", .skills = {1}});
  world.Join(2, {.name = "bob", .x = 1.0f});
  world.Join(3, {.name = "carol", .x = -1.0f, .skills = {1}});
  StartDuel(world);

  // Carol picks alice, who isn't hers to fight, and alice picks carol, who
  // isn't bob: neither is picked, and each swings at the air.
  world.Pick(3, 0, 1);
  world.Pick(1, 0, 3);
  EXPECT_EQ(world.players().at(3).target_player, 0u);
  EXPECT_EQ(world.players().at(1).target_player, 0u);
  world.SetInput(1, 0.0f, 0.0f, false, true);
  world.SetInput(3, 0.0f, 0.0f, false, true);
  Wait(world, std::chrono::seconds(1));
  EXPECT_EQ(world.players().at(1).health, 100);
  EXPECT_EQ(world.players().at(2).health, 100);
  EXPECT_EQ(world.players().at(3).health, 100);

  // Alice's whirl hits all around her: bob, and not carol, beside her too. So
  // does carol's, and it hits neither.
  world.SetInput(1, 0.0f, 0.0f, false);
  world.SetInput(3, 0.0f, 0.0f, false);
  Wait(world, std::chrono::seconds(2));
  world.UseSkill(1, 1);
  world.UseSkill(3, 1);
  Wait(world, std::chrono::seconds(1));
  EXPECT_EQ(world.players().at(2).health, 100 - 25);
  EXPECT_EQ(world.players().at(1).health, 100);
  EXPECT_EQ(world.players().at(3).health, 100);
}

TEST(Duels, ADuelEndsAtOneHealthAndNobodyDies) {
  nanommo::World world(1, nanommo::Terrain(), Swords());
  world.Join(1, {.name = "alice"});
  world.Join(2, {.name = "bob", .x = 1.0f, .health = 10});
  StartDuel(world);

  // Alice's fifteen would leave bob nothing, so it leaves him one, and he's
  // lost.
  world.Pick(1, 0, 2);
  world.SetInput(1, 0.0f, 0.0f, false, true);
  Wait(world, std::chrono::seconds(1));
  const nanommo::Player& bob = world.players().at(2);
  EXPECT_EQ(bob.health, 1);
  EXPECT_NE(bob.motion, nanommo::Motion::kDead);
  EXPECT_EQ(world.DuelOf(2).state(), nanommo::protocol::DUEL_NONE);
  const std::vector<nanommo::Notice> notices = world.TakeNotices();
  ASSERT_EQ(notices.size(), 2u);
  EXPECT_EQ(notices[0].player, 2u);
  EXPECT_EQ(notices[0].text, "alice won the duel.");
  EXPECT_EQ(notices[1].player, 1u);
  EXPECT_EQ(notices[1].text, "You won the duel against bob.");

  // It's over, so her swings go at the air, and his health stays as the duel
  // left it.
  EXPECT_EQ(world.players().at(1).target_player, 0u);
  Wait(world, std::chrono::seconds(4));
  EXPECT_EQ(bob.health, 1);
}

TEST(Duels, ASwingAtWhereTheyWereMissesWithoutCompensation) {
  nanommo::World world(1, nanommo::Terrain(), Swords());
  world.Join(1, {.name = "alice"});
  world.Join(2, {.name = "bob", .x = 1.0f});
  StartDuel(world);

  // Alice's client draws bob 100 milliseconds in the past, and with a round
  // trip of 50 to the server on top, the latest she's seen of him as her blow
  // lands is where he was 150 milliseconds before: in reach. The server
  // checks where he is now, and it misses.
  SwingAsBobRunsOff(world);
  EXPECT_EQ(world.players().at(2).health, 100);
}
