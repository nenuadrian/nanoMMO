#include "server/world.h"

#include <chrono>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

#include "gtest/gtest.h"
#include "server/monsters.h"
#include "server/test_content.h"
#include "terrain/terrain.h"

namespace {

constexpr std::chrono::milliseconds kTick(50);

void RunOneSecond(nanommo::World& world) {
  for (int tick = 0; tick < 20; ++tick) {
    world.Step(kTick);
  }
}

}  // namespace

TEST(World, JoinsPlayersWhereTheyAre) {
  nanommo::World world;
  world.Join(7, {.name = "alice", .x = 2.0f, .z = -1.0f});

  const nanommo::Player& player = world.players().at(7);
  EXPECT_EQ(player.name, "alice");
  EXPECT_EQ(player.x, 2.0f);
  EXPECT_EQ(player.z, -1.0f);
}

TEST(World, JoinsPlayersWithTheHealthTheyLeftWith) {
  nanommo::World world;
  world.Join(7, {.name = "alice", .experience = 100});
  world.Join(8, {.name = "maria", .health = 40});

  EXPECT_EQ(world.players().at(7).health, 120);
  EXPECT_EQ(world.players().at(8).health, 40);
}

TEST(World, WalksAtTheWalkSpeed) {
  nanommo::World world;
  const uint32_t id = 1;
  world.Join(id, {});
  const float start = world.players().at(id).x;

  world.SetInput(id, 1.0f, 0.0f, false);
  RunOneSecond(world);

  EXPECT_FLOAT_EQ(world.players().at(id).x, start + nanommo::World::kWalkSpeed);
  EXPECT_EQ(world.players().at(id).motion, nanommo::Motion::kWalk);
}

TEST(World, DiagonalsAreNoFaster) {
  nanommo::World world;
  const uint32_t id = 1;
  world.Join(id, {});
  const nanommo::Player start = world.players().at(id);

  world.SetInput(id, 1.0f, 1.0f, false);
  RunOneSecond(world);

  const nanommo::Player& end = world.players().at(id);
  EXPECT_FLOAT_EQ(std::hypot(end.x - start.x, end.z - start.z),
                  nanommo::World::kWalkSpeed);
}

TEST(World, FacesTheWayItMoves) {
  nanommo::World world;
  const uint32_t id = 1;
  world.Join(id, {});

  world.SetInput(id, 1.0f, 0.0f, false);
  world.Step(kTick);
  EXPECT_FLOAT_EQ(world.players().at(id).yaw, std::numbers::pi_v<float> / 2);

  world.SetInput(id, 0.0f, -1.0f, false);
  world.Step(kTick);
  EXPECT_FLOAT_EQ(world.players().at(id).yaw, std::numbers::pi_v<float>);
}

TEST(World, KeepsPlayersOnTheGround) {
  nanommo::World world;
  const uint32_t id = 1;
  world.Join(id, {});

  world.SetInput(id, 0.0f, -1.0f, true);
  for (int second = 0; second < 10; ++second) {
    RunOneSecond(world);
  }

  EXPECT_EQ(world.players().at(id).z, -nanommo::Terrain().edge());
  EXPECT_EQ(world.players().at(id).motion, nanommo::Motion::kIdle);
}

TEST(World, WalksTheIslandButNotIntoTheSea) {
  const nanommo::Terrain island(1);
  nanommo::World world(7, island);
  world.Join(1, {.x = 1000.0f});
  EXPECT_EQ(world.players().at(1).x, 0.0f);

  world.SetInput(1, 1.0f, 0.0f, true);
  for (int second = 0; second < 120; ++second) {
    RunOneSecond(world);
    const nanommo::Player& player = world.players().at(1);
    ASSERT_TRUE(island.Walkable(player.x, player.z));
  }
  EXPECT_GT(world.players().at(1).x, 50.0f);
  EXPECT_EQ(world.players().at(1).motion, nanommo::Motion::kIdle);
}

TEST(World, IgnoresInputThatIsNotANumber) {
  nanommo::World world;
  const uint32_t id = 1;
  world.Join(id, {});
  const float start = world.players().at(id).x;

  world.SetInput(id, std::numeric_limits<float>::quiet_NaN(), 0.0f, false);
  RunOneSecond(world);

  EXPECT_EQ(world.players().at(id).x, start);
}

TEST(World, SpawnsMonstersWithIdsOfTheirOwn) {
  nanommo::World world;

  const uint32_t first = world.Spawn(
      nanommo::MonsterOf(nanommo::testing::Skeleton(), 2.0f, -1.0f));
  const uint32_t second = world.Spawn(
      nanommo::MonsterOf(nanommo::testing::Skeleton(), -3.0f, 0.0f));

  EXPECT_EQ(first, 1u);
  EXPECT_EQ(second, 2u);
  EXPECT_EQ(world.monsters().at(first).home_x, 2.0f);
}

TEST(World, SwingsAtTheNearestMonsterInReach) {
  nanommo::World world(1, nanommo::Terrain(),
                       nanommo::testing::With(nanommo::testing::Sword()));
  world.Join(1, {});
  const uint32_t near =
      world.Spawn(nanommo::MonsterOf(nanommo::testing::Skeleton(), 1.0f, 0.0f));
  const uint32_t far = world.Spawn(
      nanommo::MonsterOf(nanommo::testing::Skeleton(), 0.0f, -1.5f));

  world.SetInput(1, 0.0f, 0.0f, false, true);
  world.Step(kTick);

  const nanommo::Player& player = world.players().at(1);
  EXPECT_EQ(player.motion, nanommo::Motion::kAttack);
  EXPECT_FLOAT_EQ(player.yaw, std::numbers::pi_v<float> / 2);
  RunOneSecond(world);
  EXPECT_EQ(world.monsters().at(near).health, 50 - 15);
  EXPECT_EQ(world.monsters().at(far).health, 50);
}

TEST(World, KillsMonstersForTheirExperienceAndTheyComeBack) {
  nanommo::World world(1, nanommo::Terrain(),
                       nanommo::testing::With(nanommo::testing::Sword()));
  world.Join(1, {.experience = 60});
  nanommo::Monster weak =
      nanommo::MonsterOf(nanommo::testing::Skeleton(), 1.0f, 0.0f);
  weak.max_health = 10;
  const uint32_t id = world.Spawn(weak);

  world.SetInput(1, 0.0f, 0.0f, false, true);
  RunOneSecond(world);

  EXPECT_EQ(world.monsters().at(id).motion, nanommo::Motion::kDead);
  EXPECT_EQ(world.players().at(1).experience, 110u);
  EXPECT_EQ(world.players().at(1).health, 120);

  world.SetInput(1, 0.0f, 0.0f, false);
  for (int second = 0; second < nanommo::World::kRespawnTime.count();
       ++second) {
    RunOneSecond(world);
  }
  EXPECT_NE(world.monsters().at(id).motion, nanommo::Motion::kDead);
  EXPECT_EQ(world.monsters().at(id).health, 10);
}

TEST(World, UsesTheSkillsTheyKnowOnceTheyreReady) {
  nanommo::World world(1, nanommo::Terrain(),
                       nanommo::testing::With(nanommo::testing::Sword(),
                                              nanommo::testing::Kick()));
  world.Join(1, {});
  world.Join(2, {.experience = 100, .skills = {1}});
  const uint32_t skeleton =
      world.Spawn(nanommo::MonsterOf(nanommo::testing::Skeleton(), 1.0f, 0.0f));

  // The second player knows the kick, and the first doesn't.
  world.UseSkill(1, 1);
  world.UseSkill(2, 1);
  world.Step(kTick);
  EXPECT_EQ(world.players().at(1).motion, nanommo::Motion::kIdle);
  EXPECT_EQ(world.players().at(2).motion, nanommo::Motion::kSkill);
  for (int tick = 0; tick < 10; ++tick) {
    world.Step(kTick);
  }
  const nanommo::Monster& kicked = world.monsters().at(skeleton);
  EXPECT_EQ(kicked.health, 50 - 10 - 5);
  EXPECT_FLOAT_EQ(kicked.x, 4.0f);

  // Out of its sight, it goes back for the player who kicked it.
  RunOneSecond(world);
  EXPECT_EQ(world.monsters().at(skeleton).prey, 2u);

  // Then the player can't kick again until the cooldown's over.
  world.UseSkill(2, 1);
  world.Step(kTick);
  EXPECT_NE(world.players().at(2).motion, nanommo::Motion::kSkill);
  for (int second = 0; second < 5; ++second) {
    RunOneSecond(world);
  }
  world.UseSkill(2, 1);
  world.Step(kTick);
  EXPECT_EQ(world.players().at(2).motion, nanommo::Motion::kSkill);
}

TEST(World, LearnsSkillsFromATrainerFromTheirLevel) {
  nanommo::World world(1, nanommo::Terrain(),
                       nanommo::testing::With(nanommo::testing::Sword(),
                                              nanommo::testing::Kick()));
  const uint32_t trainer =
      world.Spawn(nanommo::Npc{.name = "Trainer", .x = 3.0f, .teaches = {1}});
  world.Join(1, {.experience = 100});
  world.Join(2, {});
  world.Join(3, {.x = -2.0f, .experience = 100});

  // At level two, three metres from the trainer, the first player learns the
  // kick, once, and can use it straight away.
  EXPECT_TRUE(world.Learn(1, trainer, 1).ok());
  EXPECT_EQ(world.Learn(1, trainer, 1).message(), "You know Kick already.");
  world.UseSkill(1, 1);
  world.Step(kTick);
  EXPECT_EQ(world.players().at(1).motion, nanommo::Motion::kSkill);

  EXPECT_EQ(world.Learn(2, trainer, 1).message(),
            "You can learn Kick from level 2.");
  EXPECT_FALSE(world.players().at(2).skills.contains(1));
  EXPECT_EQ(world.Learn(3, trainer, 1).message(),
            "You're too far away to learn from the Trainer.");
}

TEST(World, MonstersKillPlayersWhoComeBackInTheMiddle) {
  nanommo::World world;
  world.Join(1, {.x = 2.0f});
  world.Spawn(nanommo::MonsterOf(nanommo::testing::Skeleton(), 3.0f, 0.0f));

  for (int second = 0; second < 18; ++second) {
    RunOneSecond(world);
  }
  EXPECT_EQ(world.players().at(1).health, 0);
  EXPECT_EQ(world.players().at(1).motion, nanommo::Motion::kDead);

  for (int second = 0; second < nanommo::World::kRespawnTime.count();
       ++second) {
    RunOneSecond(world);
  }
  const nanommo::Player& player = world.players().at(1);
  EXPECT_EQ(player.x, 0.0f);
  EXPECT_EQ(player.health, 100);
}

namespace {

constexpr uint32_t kBone = 3;
constexpr uint32_t kCap = 4;
constexpr uint32_t kBlade = 5;

nanommo::items::Stack One(uint32_t item) {
  nanommo::items::Stack stack;
  stack.set_item(item);
  stack.set_count(1);
  return stack;
}

// A player who's had a bag, with the item in it, or worn.
nanommo::Player With(uint32_t item, bool worn = false) {
  const nanommo::ItemTable gear = nanommo::testing::Gear();
  nanommo::Player player;
  if (worn) {
    player.inventory.Wear(gear, One(item));
  } else {
    player.inventory.Add(gear, One(item));
  }
  return player;
}

}  // namespace

TEST(World, GivesEachNewPlayerTheKitOnce) {
  nanommo::World world(1, nanommo::Terrain(),
                       nanommo::testing::With(nanommo::testing::Gear()));
  world.Join(1, {});
  const nanommo::Player& player = world.players().at(1);
  EXPECT_EQ(nanommo::MainHandOf(player), 1u);
  ASSERT_NE(player.inventory.At(nanommo::InBag(0)), nullptr);
  EXPECT_EQ(player.inventory.At(nanommo::InBag(0))->count(), 3u);

  // A trip through a gate joins them to another copy, with what they had, and
  // someone who's thrown it all away doesn't get another.
  nanommo::Player through = world.players().at(1);
  world.Leave(1);
  world.Join(1, through);
  EXPECT_EQ(world.players().at(1).inventory.items().bag_size(), 1);
  ASSERT_TRUE(world.DestroyItem(1, nanommo::InBag(0)).ok());
  ASSERT_TRUE(
      world.DestroyItem(1, nanommo::Worn(nanommo::items::SLOT_MAIN_HAND)).ok());
  through = world.players().at(1);
  world.Leave(1);
  world.Join(1, through);
  EXPECT_EQ(world.players().at(1).inventory.items().bag_size(), 0);
  EXPECT_EQ(nanommo::MainHandOf(world.players().at(1)), 0u);
}

TEST(World, FightsWithWhatTheyWear) {
  nanommo::World world(1, nanommo::Terrain(),
                       nanommo::testing::With(nanommo::testing::Gear()));
  // The blade does twenty, one more for being worn, and this one rolled three
  // more.
  nanommo::items::Stack blade = One(kBlade);
  nanommo::items::Modifier* roll = blade.add_rolls();
  roll->set_stat(nanommo::items::STAT_DAMAGE);
  roll->set_amount(3);
  nanommo::Player alice;
  alice.inventory.Add(nanommo::testing::Gear(), blade);
  world.Join(1, alice);
  ASSERT_TRUE(world
                  .MoveItem(1, nanommo::InBag(0),
                            nanommo::Worn(nanommo::items::SLOT_MAIN_HAND))
                  .ok());
  const uint32_t skeleton =
      world.Spawn(nanommo::MonsterOf(nanommo::testing::Skeleton(), 1.0f, 0.0f));

  world.SetInput(1, 0.0f, 0.0f, false, true);
  RunOneSecond(world);
  EXPECT_EQ(world.monsters().at(skeleton).health, 50 - 24);
}

TEST(World, SwingsOnlyWithAWeaponInHand) {
  nanommo::World world(1, nanommo::Terrain(),
                       nanommo::testing::With(nanommo::testing::Gear()));
  world.Join(1, With(kBone));
  world.Join(2, {.z = 0.5f});
  const uint32_t skeleton =
      world.Spawn(nanommo::MonsterOf(nanommo::testing::Skeleton(), 1.0f, 0.0f));

  world.SetInput(1, 0.0f, 0.0f, false, true);
  world.SetInput(2, 0.0f, 0.0f, false, true);
  world.Step(kTick);
  EXPECT_NE(world.players().at(1).motion, nanommo::Motion::kAttack);
  EXPECT_EQ(world.players().at(2).motion, nanommo::Motion::kAttack);

  // Putting the sword away ends the swing, before it lands.
  ASSERT_TRUE(world
                  .MoveItem(2, nanommo::Worn(nanommo::items::SLOT_MAIN_HAND),
                            nanommo::InBag(5))
                  .ok());
  EXPECT_NE(world.players().at(2).motion, nanommo::Motion::kAttack);
  RunOneSecond(world);
  EXPECT_EQ(world.monsters().at(skeleton).health, 50);
}

TEST(World, ArmourTakesTheBlowsAndHealthComesWithIt) {
  // Two worlds that go the same way, but for the cap.
  const auto fight = [](nanommo::Player player) {
    nanommo::World world(1, nanommo::Terrain(),
                         nanommo::testing::With(nanommo::testing::Gear()));
    player.x = 2.0f;
    world.Join(1, player);
    world.Spawn(nanommo::MonsterOf(nanommo::testing::Skeleton(), 3.0f, 0.0f));
    for (int second = 0; second < 4; ++second) {
      RunOneSecond(world);
    }
    return world.players().at(1).health;
  };
  // The cap adds ten health and two armour, which takes the skeleton's five
  // down to three.
  const int capped = fight(With(kCap, true));
  const int bare = fight(With(kBone));
  EXPECT_LT(capped, 110);
  EXPECT_EQ((110 - capped) % 3, 0);
  EXPECT_EQ((110 - capped) / 3, (100 - bare) / 5);

  // Taking it off takes its health with it.
  nanommo::World world(1, nanommo::Terrain(),
                       nanommo::testing::With(nanommo::testing::Gear()));
  world.Join(1, With(kCap, true));
  EXPECT_EQ(world.players().at(1).health, 110);
  EXPECT_EQ(nanommo::MaxHealthOf(world.players().at(1)), 110);
  ASSERT_TRUE(world
                  .MoveItem(1, nanommo::Worn(nanommo::items::SLOT_HEAD),
                            nanommo::InBag(1))
                  .ok());
  EXPECT_EQ(world.players().at(1).health, 100);
}

TEST(World, UsesWhatsInTheBagOnceItsCooledDown) {
  nanommo::World world(1, nanommo::Terrain(),
                       nanommo::testing::With(nanommo::testing::Gear()));
  world.Join(1, {.health = 50});
  world.Join(2, With(kBone));
  const auto potions = [&world] {
    return world.players().at(1).inventory.At(nanommo::InBag(0))->count();
  };

  ASSERT_TRUE(world.UseItem(1, 0).ok());
  EXPECT_EQ(world.players().at(1).health, 90);
  EXPECT_EQ(potions(), 2u);
  EXPECT_EQ(world.UseItem(1, 0).message(),
            "You can use another Potion in 10 s.");
  for (int second = 0; second < 10; ++second) {
    RunOneSecond(world);
  }
  ASSERT_TRUE(world.UseItem(1, 0).ok());
  EXPECT_EQ(world.players().at(1).health, 100);
  EXPECT_EQ(potions(), 1u);
  for (int second = 0; second < 10; ++second) {
    RunOneSecond(world);
  }
  EXPECT_EQ(world.UseItem(1, 0).message(), "You're at full health already.");
  EXPECT_EQ(potions(), 1u);

  EXPECT_EQ(world.UseItem(1, 5).message(), "There's nothing there.");
  EXPECT_EQ(world.UseItem(2, 0).message(), "You can't use the Bone.");
}

namespace {

// A skeleton that always drops two bones and a blade, and whose health one
// swing takes.
nanommo::MonsterTable Looted() {
  nanommo::monsters::Kind kind = nanommo::testing::Skeleton();
  kind.set_max_health(10);
  nanommo::monsters::Drop* bones = kind.add_loot();
  bones->set_item(kBone);
  bones->set_chance(1.0f);
  bones->set_min(2);
  bones->set_max(2);
  nanommo::monsters::Drop* blade = kind.add_loot();
  blade->set_item(kBlade);
  blade->set_chance(1.0f);
  return {.kinds = {{1, kind}}};
}

// Kills the looted skeleton in front of the player.
void Kill(nanommo::World& world, uint32_t id) {
  nanommo::monsters::Kind kind = Looted().kinds.at(1);
  world.Spawn(nanommo::MonsterOf(kind, 1.0f, 0.0f));
  world.SetInput(id, 0.0f, 0.0f, false, true);
  RunOneSecond(world);
  world.SetInput(id, 0.0f, 0.0f, false);
}

}  // namespace

TEST(World, PutsWhatAMonsterDropsInItsKillersBag) {
  nanommo::World world(
      1, nanommo::Terrain(),
      nanommo::testing::With(nanommo::testing::Gear(), {}, Looted()));
  world.Join(1, {});
  Kill(world, 1);

  // The kit's potions are in the first slot, so the loot's in the next two.
  const nanommo::Inventory& bag = world.players().at(1).inventory;
  ASSERT_NE(bag.At(nanommo::InBag(1)), nullptr);
  EXPECT_EQ(bag.At(nanommo::InBag(1))->item(), kBone);
  EXPECT_EQ(bag.At(nanommo::InBag(1))->count(), 2u);
  const nanommo::items::Stack* blade = bag.At(nanommo::InBag(2));
  ASSERT_NE(blade, nullptr);
  EXPECT_EQ(blade->item(), kBlade);
  // It rolled its damage, from two to four.
  ASSERT_EQ(blade->rolls_size(), 1);
  EXPECT_EQ(blade->rolls(0).stat(), nanommo::items::STAT_DAMAGE);
  EXPECT_GE(blade->rolls(0).amount(), 2);
  EXPECT_LE(blade->rolls(0).amount(), 4);

  const std::vector<nanommo::Notice> notices = world.TakeNotices();
  ASSERT_EQ(notices.size(), 1u);
  EXPECT_EQ(notices[0].player, 1u);
  EXPECT_EQ(notices[0].text, "The Skeleton dropped 2 Bones and a Blade.");
  EXPECT_TRUE(world.TakeNotices().empty());
}

TEST(World, LeavesWhatDoesntFitInTheBag) {
  nanommo::World world(
      1, nanommo::Terrain(),
      nanommo::testing::With(nanommo::testing::Gear(), {}, Looted()));
  // A sword in hand, and a cap in each slot of the bag.
  nanommo::Player alice = With(1, true);
  for (uint32_t slot = 0; slot < nanommo::kBagSlots; ++slot) {
    alice.inventory.Add(nanommo::testing::Gear(), One(kCap));
  }
  world.Join(1, alice);
  Kill(world, 1);

  EXPECT_EQ(world.players().at(1).inventory.items().bag_size(),
            static_cast<int>(nanommo::kBagSlots));
  const std::vector<nanommo::Notice> notices = world.TakeNotices();
  ASSERT_EQ(notices.size(), 1u);
  EXPECT_EQ(notices[0].text,
            "Your bag is full, so you left 2 Bones and a Blade behind.");
}
