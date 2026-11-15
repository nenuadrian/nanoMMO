#include "server/trades.h"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "protocol/places.h"
#include "server/test_content.h"
#include "server/world.h"
#include "terrain/terrain.h"

namespace {

constexpr uint32_t kPotion = 2;
constexpr uint32_t kBone = 3;
constexpr uint32_t kCap = 4;

nanommo::items::Stack Of(uint32_t item, uint32_t count = 1) {
  nanommo::items::Stack stack;
  stack.set_item(item);
  stack.set_count(count);
  return stack;
}

nanommo::protocol::TradeOffer Offer(const std::vector<uint32_t>& slots,
                                    uint32_t gold = 0) {
  nanommo::protocol::TradeOffer offer;
  offer.mutable_bag()->Add(slots.begin(), slots.end());
  offer.set_gold(gold);
  return offer;
}

// A trade between players 1 and 2, as each asked the other.
nanommo::Trades Open() {
  nanommo::Trades trades;
  trades.Ask(1, 2);
  trades.Ask(2, 1);
  trades.TakeNews();
  return trades;
}

}  // namespace

TEST(Trades, OpensOnceEachHasAskedTheOther) {
  nanommo::Trades trades;
  EXPECT_FALSE(trades.Ask(1, 2));
  EXPECT_EQ(trades.With(1), 0u);
  // Asking someone else takes the first asking back.
  EXPECT_FALSE(trades.Ask(1, 3));
  EXPECT_FALSE(trades.Ask(2, 1));
  EXPECT_TRUE(trades.TakeNews().empty());

  EXPECT_TRUE(trades.Ask(1, 2));
  EXPECT_EQ(trades.With(1), 2u);
  EXPECT_EQ(trades.With(2), 1u);
  EXPECT_EQ(trades.TakeNews(), (std::set<uint32_t>{1, 2}));
}

TEST(Trades, ChangingAnOfferMakesNeitherSideReady) {
  const nanommo::ItemTable gear = nanommo::testing::Gear();
  nanommo::Inventory alice;
  alice.Add(gear, Of(kBone, 5));
  alice.AddGold(30);
  nanommo::Inventory bob;
  bob.Add(gear, Of(kCap));
  nanommo::Trades trades = Open();

  ASSERT_TRUE(trades.Offer(1, alice, gear, Offer({0}, 20)).ok());
  EXPECT_EQ(trades.SideOf(1).bag().at(0).count(), 5u);
  EXPECT_EQ(trades.SideOf(1).gold(), 20u);
  ASSERT_TRUE(trades.Offer(2, bob, gear, Offer({0})).ok());
  EXPECT_EQ(trades.Ready(2, true).value_or(true), false);
  EXPECT_TRUE(trades.SideOf(2).ready());
  EXPECT_EQ(trades.TakeNews(), (std::set<uint32_t>{1, 2}));

  // Alice offers less gold, so bob has something new to look at.
  ASSERT_TRUE(trades.Offer(1, alice, gear, Offer({0}, 10)).ok());
  EXPECT_FALSE(trades.SideOf(2).ready());

  // What isn't there they can't offer, and an offer that's refused leaves
  // the one before.
  EXPECT_EQ(trades.Offer(1, alice, gear, Offer({3})).message(),
            "There's nothing there.");
  EXPECT_EQ(trades.Offer(1, alice, gear, Offer({}, 31)).message(),
            "You haven't that much gold.");
  EXPECT_EQ(trades.SideOf(1).gold(), 10u);
  EXPECT_EQ(trades.Offer(3, bob, gear, Offer({0})).message(),
            "You're not trading with anyone.");
}

TEST(Trades, OnceBothAreReadyEachGetsWhatTheOtherOffered) {
  const nanommo::ItemTable gear = nanommo::testing::Gear();
  nanommo::Inventory alice;
  alice.Add(gear, Of(kBone, 5));
  alice.AddGold(30);
  nanommo::Inventory bob;
  bob.Add(gear, Of(kPotion, 3));
  bob.Add(gear, Of(kCap));
  nanommo::Trades trades = Open();
  ASSERT_TRUE(trades.Offer(1, alice, gear, Offer({0}, 20)).ok());
  ASSERT_TRUE(trades.Offer(2, bob, gear, Offer({1})).ok());

  ASSERT_EQ(trades.Ready(1, true).value_or(true), false);
  ASSERT_EQ(trades.Ready(2, true).value_or(false), true);
  ASSERT_TRUE(trades.Complete(2, bob, alice, gear).ok());

  // Each takes what they get into the first place it goes, which can be the
  // one the other's offer left.
  ASSERT_NE(alice.At(nanommo::InBag(0)), nullptr);
  EXPECT_EQ(alice.At(nanommo::InBag(0))->item(), kCap);
  EXPECT_EQ(alice.gold(), 10u);
  ASSERT_NE(bob.At(nanommo::InBag(1)), nullptr);
  EXPECT_EQ(bob.At(nanommo::InBag(1))->item(), kBone);
  EXPECT_EQ(bob.At(nanommo::InBag(1))->count(), 5u);
  EXPECT_EQ(bob.At(nanommo::InBag(0))->count(), 3u);
  EXPECT_EQ(bob.gold(), 20u);
  // And it's over.
  EXPECT_EQ(trades.With(1), 0u);
  EXPECT_EQ(trades.With(2), 0u);
}

TEST(Trades, GivesNothingUntilBothBagsHaveRoom) {
  const nanommo::ItemTable gear = nanommo::testing::Gear();
  nanommo::Inventory alice;
  alice.Add(gear, Of(kBone, 5));
  nanommo::Inventory bob;
  for (uint32_t slot = 0; slot < nanommo::kBagSlots; ++slot) {
    bob.Add(gear, Of(kCap));
  }
  nanommo::Trades trades = Open();
  ASSERT_TRUE(trades.Offer(1, alice, gear, Offer({0})).ok());
  ASSERT_TRUE(trades.Ready(1, true).ok());
  ASSERT_TRUE(trades.Ready(2, true).ok());

  const uint32_t alices = alice.revision();
  const uint32_t bobs = bob.revision();
  EXPECT_EQ(trades.Complete(1, alice, bob, gear).message(),
            "There isn't room for the trade in one of the bags.");
  EXPECT_EQ(alice.revision(), alices);
  EXPECT_EQ(bob.revision(), bobs);
  // They're still trading, and neither is ready.
  EXPECT_EQ(trades.With(1), 2u);
  EXPECT_FALSE(trades.SideOf(1).ready());
  EXPECT_FALSE(trades.SideOf(2).ready());

  // A cap of bob's for the bones leaves him the room for them.
  ASSERT_TRUE(trades.Offer(2, bob, gear, Offer({0})).ok());
  ASSERT_TRUE(trades.Ready(1, true).ok());
  ASSERT_TRUE(trades.Ready(2, true).ok());
  ASSERT_TRUE(trades.Complete(1, alice, bob, gear).ok());
  EXPECT_EQ(alice.At(nanommo::InBag(0))->item(), kCap);
  EXPECT_EQ(bob.At(nanommo::InBag(0))->item(), kBone);
}

TEST(Trades, TakesAsManyAsWereOffered) {
  const nanommo::ItemTable gear = nanommo::testing::Gear();
  nanommo::Inventory alice;
  alice.Add(gear, Of(kBone, 5));
  nanommo::Inventory bob;
  nanommo::Trades trades = Open();
  ASSERT_TRUE(trades.Offer(1, alice, gear, Offer({0})).ok());
  // A kill puts more bones onto the stack she offered, which stay hers.
  alice.Add(gear, Of(kBone, 10));
  ASSERT_TRUE(trades.Ready(1, true).ok());
  ASSERT_TRUE(trades.Ready(2, true).ok());

  ASSERT_TRUE(trades.Complete(1, alice, bob, gear).ok());
  EXPECT_EQ(alice.At(nanommo::InBag(0))->count(), 10u);
  EXPECT_EQ(bob.At(nanommo::InBag(0))->count(), 5u);
}

TEST(Trades, EndingItEndsTheAskingToo) {
  nanommo::Trades trades;
  trades.Ask(1, 2);
  EXPECT_EQ(trades.End(1), 0u);
  // Alice's asking went with it, so bob's asking her opens nothing.
  EXPECT_FALSE(trades.Ask(2, 1));
  EXPECT_TRUE(trades.Ask(1, 2));
  trades.TakeNews();

  EXPECT_EQ(trades.End(2), 1u);
  EXPECT_EQ(trades.With(1), 0u);
  EXPECT_EQ(trades.TakeNews(), (std::set<uint32_t>{1, 2}));
}

TEST(Trades, PlayersTradeWithinReachInTheirWorld) {
  nanommo::World world(1, nanommo::Terrain(),
                       nanommo::testing::With(nanommo::testing::Gear()));
  // Each with the kit: a sword, and three potions in the first slot.
  world.Join(1, {.name = "alice"});
  world.Join(2, {.name = "bob", .x = 5.0f});
  EXPECT_EQ(world.AskToTrade(1, "bob").message(),
            "You're too far away to trade with bob.");
  EXPECT_EQ(world.AskToTrade(1, "carol").message(),
            "Nobody of that name is here.");
  world.Leave(2);
  world.Join(2, {.name = "bob", .x = 2.0f});

  // By their name, whatever its case.
  ASSERT_TRUE(world.AskToTrade(1, "BOB").ok());
  std::vector<nanommo::Notice> notices = world.TakeNotices();
  ASSERT_EQ(notices.size(), 2u);
  EXPECT_EQ(notices[0].player, 2u);
  EXPECT_EQ(notices[0].text,
            "alice asks to trade. Type /trade alice to say yes.");
  ASSERT_TRUE(world.AskToTrade(2, "alice").ok());

  ASSERT_TRUE(world.OfferToTrade(1, Offer({0})).ok());
  ASSERT_TRUE(world.ReadyToTrade(1, true).ok());
  ASSERT_TRUE(world.ReadyToTrade(2, true).ok());
  // Her three potions went onto his.
  EXPECT_EQ(world.players().at(1).inventory.At(nanommo::InBag(0)), nullptr);
  EXPECT_EQ(world.players().at(2).inventory.At(nanommo::InBag(0))->count(), 6u);
  notices = world.TakeNotices();
  // Bob, who was ready last, hears first.
  ASSERT_EQ(notices.size(), 2u);
  EXPECT_EQ(notices[0].player, 2u);
  EXPECT_EQ(notices[0].text, "You traded with alice.");
  EXPECT_EQ(notices[1].player, 1u);
  EXPECT_EQ(notices[1].text, "You traded with bob.");

  // Leaving calls a trade off.
  ASSERT_TRUE(world.AskToTrade(1, "bob").ok());
  ASSERT_TRUE(world.AskToTrade(2, "alice").ok());
  world.TakeNotices();
  world.Leave(2);
  notices = world.TakeNotices();
  ASSERT_EQ(notices.size(), 1u);
  EXPECT_EQ(notices[0].text, "bob has gone, so the trade's off.");
  EXPECT_EQ(world.ReadyToTrade(1, true).message(),
            "You're not trading with anyone.");
}
