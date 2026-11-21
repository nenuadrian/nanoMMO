#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "protocol/places.h"
#include "server/monsters.h"
#include "server/test_content.h"
#include "server/world.h"
#include "terrain/terrain.h"

namespace {

constexpr std::chrono::milliseconds kTick(50);

constexpr uint32_t kSword = 1;
constexpr uint32_t kBone = 3;
constexpr uint32_t kCap = 4;
constexpr uint32_t kBlade = 5;

constexpr uint32_t kBindABlade = 1;
constexpr uint32_t kCarveACap = 2;

// The tests' gear, and two recipes: a sword and ten bones bind a blade, in
// ten seconds, and five bones carve a cap, in one, from level 2.
std::shared_ptr<const nanommo::Content> Recipes() {
  return std::make_shared<const nanommo::Content>(
      nanommo::Content{.items = nanommo::testing::Gear(),
                       .recipes = *nanommo::ParseRecipes(R"pb(
                         recipes {
                           id: 1
                           name: "Bind a Blade"
                           inputs { item: 1 count: 1 }
                           inputs { item: 3 count: 10 }
                           output { item: 5 count: 1 }
                           seconds: 10
                           level: 1
                         }
                         recipes {
                           id: 2
                           name: "Carve a Cap"
                           inputs { item: 3 count: 5 }
                           output { item: 4 count: 1 }
                           seconds: 1
                           level: 2
                         }
                       )pb"),
                       .skills = nanommo::testing::WithStrike({})});
}

nanommo::items::Stack Of(uint32_t item, uint32_t count = 1) {
  nanommo::items::Stack stack;
  stack.set_item(item);
  stack.set_count(count);
  return stack;
}

// A player with a sword in the first slot of their bag, and bones in the next.
nanommo::Player Crafter(std::string name, uint32_t bones = 12) {
  const nanommo::ItemTable gear = nanommo::testing::Gear();
  nanommo::Player player{.name = std::move(name)};
  player.inventory.Add(gear, Of(kSword));
  player.inventory.Add(gear, Of(kBone, bones));
  return player;
}

void RunFor(nanommo::World& world, std::chrono::seconds time) {
  for (auto run = std::chrono::nanoseconds::zero(); run < time; run += kTick) {
    world.Step(kTick);
  }
}

bool Same(const nanommo::Inventory& a, const nanommo::Inventory& b) {
  return google::protobuf::util::MessageDifferencer::Equals(a.items(),
                                                            b.items());
}

// What the world told the player since the last time.
std::vector<std::string> Lines(nanommo::World& world, uint32_t id) {
  std::vector<std::string> lines;
  for (const nanommo::Notice& notice : world.TakeNotices()) {
    if (notice.player == id) {
      lines.push_back(notice.text);
    }
  }
  return lines;
}

// Whether the player, left alone, binds the blade, into their first slot.
bool BindsTheBlade(nanommo::World& world, uint32_t id) {
  if (!world.Craft(id, kBindABlade).ok()) {
    return false;
  }
  RunFor(world, std::chrono::seconds(10));
  const nanommo::items::Stack* blade =
      world.players().at(id).inventory.At(nanommo::InBag(0));
  return blade != nullptr && blade->item() == kBlade;
}

}  // namespace

TEST(Crafting, MakesWhatTheRecipeMakesOnceItsTimeIsUp) {
  nanommo::World world(1, nanommo::Terrain(), Recipes());
  world.Join(1, Crafter("alice"));
  const nanommo::Inventory before = world.players().at(1).inventory;
  const uint64_t sword = before.At(nanommo::InBag(0))->serial();

  ASSERT_TRUE(world.Craft(1, kBindABlade).ok());
  RunFor(world, std::chrono::seconds(9));
  EXPECT_EQ(world.players().at(1).crafting, kBindABlade);
  EXPECT_TRUE(Same(world.players().at(1).inventory, before));

  // The sword goes, serial and all, and ten of the bones, and the blade goes
  // into the sword's slot, with a serial of its own and its roll, as a
  // dropped one would.
  RunFor(world, std::chrono::seconds(1));
  const nanommo::Player& alice = world.players().at(1);
  EXPECT_EQ(alice.crafting, 0u);
  const nanommo::items::Stack* blade = alice.inventory.At(nanommo::InBag(0));
  ASSERT_NE(blade, nullptr);
  EXPECT_EQ(blade->item(), kBlade);
  EXPECT_NE(blade->serial(), 0u);
  EXPECT_NE(blade->serial(), sword);
  ASSERT_EQ(blade->rolls_size(), 1);
  EXPECT_GE(blade->rolls(0).amount(), 2);
  EXPECT_LE(blade->rolls(0).amount(), 4);
  EXPECT_EQ(alice.inventory.At(nanommo::InBag(1))->count(), 2u);
  EXPECT_EQ(alice.inventory.items().bag_size(), 2);
  EXPECT_GT(alice.inventory.revision(), before.revision());
  EXPECT_EQ(Lines(world, 1), std::vector<std::string>{"You made a Blade."});

  // For the analytics: what she made, and how many.
  const std::vector<nanommo::Event> events = world.TakeEvents();
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].kind, nanommo::Event::Kind::kCraft);
  EXPECT_EQ(events[0].player, "alice");
  EXPECT_EQ(events[0].item, kBlade);
  EXPECT_EQ(events[0].count, 1u);
}

TEST(Crafting, RefusesWhatTheyCantMake) {
  nanommo::World world(1, nanommo::Terrain(), Recipes());
  world.Join(1, Crafter("alice"));
  EXPECT_EQ(world.Craft(1, 9).message(), "There's no such recipe.");

  // A cap from level 2.
  EXPECT_EQ(world.Craft(1, kCarveACap).message(),
            "You can make a Cap from level 2.");
  nanommo::Player bob = Crafter("bob");
  bob.experience = 100;
  world.Join(2, bob);
  EXPECT_TRUE(world.Craft(2, kCarveACap).ok());

  // A blade from a sword in the bag, and not the one in hand, and ten bones.
  nanommo::Player carol = Crafter("carol");
  ASSERT_TRUE(carol.inventory
                  .Move(nanommo::testing::Gear(), nanommo::InBag(0),
                        nanommo::Worn(nanommo::items::SLOT_MAIN_HAND))
                  .ok());
  world.Join(3, carol);
  EXPECT_EQ(world.Craft(3, kBindABlade).message(),
            "You need a Sword in your bag to make a Blade.");
  world.Join(4, Crafter("dave", 9));
  EXPECT_EQ(world.Craft(4, kBindABlade).message(),
            "You need 10 Bones in your bag to make a Blade.");

  // Not while they're dead.
  nanommo::Player erin = Crafter("erin");
  erin.motion = nanommo::Motion::kDead;
  world.Join(5, erin);
  EXPECT_EQ(world.Craft(5, kBindABlade).message(),
            "You can't, while you're dead.");

  // One thing at a time.
  ASSERT_TRUE(world.Craft(1, kBindABlade).ok());
  EXPECT_EQ(world.Craft(1, kBindABlade).message(),
            "You're making something already.");
  world.StopCrafting(1);
  EXPECT_EQ(Lines(world, 1),
            std::vector<std::string>{"You stopped making a Blade."});
  EXPECT_TRUE(BindsTheBlade(world, 1));
}

TEST(Crafting, NeedsRoomForWhatItMakesOnceWhatItTakesIsGone) {
  // Bones in the first slot of the bag, and a cap in each of the others.
  const auto full = [](uint32_t bones) {
    const nanommo::ItemTable gear = nanommo::testing::Gear();
    nanommo::Player player{.experience = 100};
    player.inventory.Add(gear, Of(kBone, bones));
    for (uint32_t slot = 1; slot < nanommo::kBagSlots; ++slot) {
      player.inventory.Add(gear, Of(kCap));
    }
    return player;
  };
  nanommo::World world(1, nanommo::Terrain(), Recipes());
  world.Join(1, full(7));
  EXPECT_EQ(world.Craft(1, kCarveACap).message(),
            "Your bag is full. Make room for a Cap first.");

  // Five bones, all it takes, leave their slot for the cap.
  world.Join(2, full(5));
  ASSERT_TRUE(world.Craft(2, kCarveACap).ok());
  RunFor(world, std::chrono::seconds(1));
  EXPECT_EQ(world.players().at(2).inventory.At(nanommo::InBag(0))->item(),
            kCap);
}

TEST(Crafting, WhatsOfferedInATradeIsntThereToMakeFrom) {
  nanommo::World world(1, nanommo::Terrain(), Recipes());
  world.Join(1, Crafter("alice"));
  world.Join(2, {.name = "bob", .x = 1.0f});
  ASSERT_TRUE(world.AskToTrade(1, "bob").ok());
  ASSERT_TRUE(world.AskToTrade(2, "alice").ok());

  nanommo::protocol::TradeOffer bones;
  bones.add_bag(1);
  ASSERT_TRUE(world.OfferToTrade(1, bones).ok());
  EXPECT_EQ(world.Craft(1, kBindABlade).message(),
            "You need 10 Bones in your bag to make a Blade.");
  ASSERT_TRUE(world.OfferToTrade(1, {}).ok());
  EXPECT_TRUE(BindsTheBlade(world, 1));
}

TEST(Crafting, ChecksTheBagAgainOnceItsMade) {
  nanommo::World world(1, nanommo::Terrain(), Recipes());
  world.Join(1, Crafter("alice"));
  ASSERT_TRUE(world.Craft(1, kBindABlade).ok());
  ASSERT_TRUE(world.DestroyItem(1, nanommo::InBag(1)).ok());
  const nanommo::Inventory before = world.players().at(1).inventory;

  RunFor(world, std::chrono::seconds(10));
  EXPECT_EQ(Lines(world, 1),
            std::vector<std::string>{
                "You couldn't make a Blade. You need 10 Bones in your bag to "
                "make a Blade."});
  EXPECT_EQ(world.players().at(1).crafting, 0u);
  EXPECT_TRUE(Same(world.players().at(1).inventory, before));
}

TEST(Crafting, MovingASwingOrASkillStopsIt) {
  nanommo::World world(1, nanommo::Terrain(), Recipes());
  world.Join(1, Crafter("alice"));
  const nanommo::Inventory before = world.players().at(1).inventory;

  ASSERT_TRUE(world.Craft(1, kBindABlade).ok());
  world.SetInput(1, 1.0f, 0.0f, false);
  world.Step(kTick);
  world.SetInput(1, 0.0f, 0.0f, false);
  EXPECT_EQ(world.players().at(1).crafting, 0u);
  EXPECT_EQ(Lines(world, 1),
            std::vector<std::string>{"You stopped making a Blade."});

  // A walk she asked for, and Space.
  ASSERT_TRUE(world.Craft(1, kBindABlade).ok());
  world.WalkTo(1, {.x = 3.0f, .z = 0.0f});
  world.Step(kTick);
  EXPECT_EQ(world.players().at(1).crafting, 0u);
  RunFor(world, std::chrono::seconds(2));
  ASSERT_TRUE(world.Craft(1, kBindABlade).ok());
  world.SetInput(1, 0.0f, 0.0f, false, true);
  world.Step(kTick);
  world.SetInput(1, 0.0f, 0.0f, false);
  EXPECT_EQ(world.players().at(1).crafting, 0u);

  // She lost nothing, and standing still, she makes it.
  EXPECT_TRUE(Same(world.players().at(1).inventory, before));
  EXPECT_TRUE(BindsTheBlade(world, 1));
}

TEST(Crafting, ABlowStopsItAndSoDoesDying) {
  // A skeleton a metre away, which comes for them.
  for (const int health : {100, 1}) {
    nanommo::World world(1, nanommo::Terrain(), Recipes());
    nanommo::Player alice = Crafter("alice");
    alice.health = health;
    world.Join(1, alice);
    const nanommo::Inventory before = world.players().at(1).inventory;
    world.Spawn(nanommo::MonsterOf(nanommo::testing::Skeleton(), 1.0f, 0.0f));
    ASSERT_TRUE(world.Craft(1, kBindABlade).ok());

    for (int tick = 0; tick < 200 && world.players().at(1).health == health;
         ++tick) {
      world.Step(kTick);
    }
    const nanommo::Player& hurt = world.players().at(1);
    EXPECT_LT(hurt.health, health);
    EXPECT_EQ(hurt.motion == nanommo::Motion::kDead, health == 1);
    EXPECT_EQ(hurt.crafting, 0u);
    EXPECT_TRUE(Same(hurt.inventory, before));
  }
}

TEST(Crafting, ATradeOrADuelStopsItOnceItStarts) {
  nanommo::World world(1, nanommo::Terrain(), Recipes());
  world.Join(1, Crafter("alice"));
  world.Join(2, {.name = "bob", .x = 1.0f});

  // Bob asking doesn't stop her, and her saying yes does.
  ASSERT_TRUE(world.Craft(1, kBindABlade).ok());
  ASSERT_TRUE(world.AskToTrade(2, "alice").ok());
  EXPECT_EQ(world.players().at(1).crafting, kBindABlade);
  ASSERT_TRUE(world.AskToTrade(1, "bob").ok());
  EXPECT_EQ(world.players().at(1).crafting, 0u);
  world.CancelTrade(1);

  ASSERT_TRUE(world.Craft(1, kBindABlade).ok());
  ASSERT_TRUE(world.AskToDuel(2, "alice").ok());
  EXPECT_EQ(world.players().at(1).crafting, kBindABlade);
  ASSERT_TRUE(world.AnswerDuel(1, true).ok());
  EXPECT_EQ(world.players().at(1).crafting, 0u);
}

TEST(Crafting, LeavingStopsIt) {
  nanommo::World world(1, nanommo::Terrain(), Recipes());
  world.Join(1, Crafter("alice"));
  ASSERT_TRUE(world.Craft(1, kBindABlade).ok());
  RunFor(world, std::chrono::seconds(5));

  // As through a gate, into another land, with all she had.
  nanommo::Player alice = world.players().at(1);
  world.Leave(1);
  world.Join(1, alice);
  EXPECT_EQ(world.players().at(1).crafting, 0u);
  RunFor(world, std::chrono::seconds(5));
  EXPECT_TRUE(Same(world.players().at(1).inventory, alice.inventory));
  EXPECT_TRUE(BindsTheBlade(world, 1));
}
