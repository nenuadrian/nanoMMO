#include "client/predicted_bag.h"

#include <cstdint>

#include "content/content.h"
#include "gtest/gtest.h"
#include "protocol/places.h"

namespace {

nanommo::ItemTable Items() {
  return nanommo::ReadContent(NANOMMO_CONTENT)->items;
}

nanommo::items::Stack Of(uint32_t item, uint32_t count = 1) {
  nanommo::items::Stack stack;
  stack.set_item(item);
  stack.set_count(count);
  return stack;
}

constexpr uint32_t kSword = 1;
constexpr uint32_t kPotion = 2;

nanommo::protocol::ItemAction Move(const nanommo::protocol::Place& from,
                                   const nanommo::protocol::Place& to) {
  nanommo::protocol::ItemAction action;
  *action.mutable_move()->mutable_from() = from;
  *action.mutable_move()->mutable_to() = to;
  return action;
}

}  // namespace

TEST(PredictedBag, MovesAnItemAtOnceUntilTheServersBagComes) {
  const nanommo::ItemTable items = Items();
  nanommo::PredictedBag predicted(items);
  nanommo::protocol::Inventory server;
  server.set_revision(4);
  (*server.mutable_bag())[0] = Of(kSword);

  predicted.Act(server, Move(nanommo::InBag(0), nanommo::InBag(5)));
  // Before the server's answered, the sword's where it was dropped.
  nanommo::protocol::Inventory shown = predicted.Shown(server, 0.05f);
  EXPECT_FALSE(shown.bag().contains(0));
  EXPECT_EQ(shown.bag().at(5).item(), kSword);

  // The server's bag at the next revision takes over.
  nanommo::protocol::Inventory answered;
  answered.set_revision(5);
  (*answered.mutable_bag())[5] = Of(kSword);
  shown = predicted.Shown(answered, 0.05f);
  EXPECT_EQ(shown.revision(), 5u);
  EXPECT_FALSE(predicted.ahead());
}

TEST(PredictedBag, PutsItBackWhenTheServerDoesntAnswer) {
  const nanommo::ItemTable items = Items();
  nanommo::PredictedBag predicted(items);
  nanommo::protocol::Inventory server;
  server.set_revision(4);
  (*server.mutable_bag())[0] = Of(kSword);
  predicted.Act(server, Move(nanommo::InBag(0), nanommo::InBag(5)));

  // As when the server refused: the same revision, past our patience.
  const nanommo::protocol::Inventory shown =
      predicted.Shown(server, nanommo::PredictedBag::kPatience + 0.1f);
  EXPECT_EQ(shown.bag().at(0).item(), kSword);
  EXPECT_FALSE(shown.bag().contains(5));
}

TEST(PredictedBag, DoesNothingTheRulesRefuse) {
  const nanommo::ItemTable items = Items();
  nanommo::PredictedBag predicted(items);
  nanommo::protocol::Inventory server;
  (*server.mutable_bag())[0] = Of(kPotion, 3);
  // A potion isn't worn, so it can't go on the head.
  predicted.Act(server, Move(nanommo::InBag(0),
                             nanommo::Worn(nanommo::items::SLOT_HEAD)));
  EXPECT_FALSE(predicted.ahead());
  // Its valid twin, into an empty slot of the bag, goes ahead.
  predicted.Act(server, Move(nanommo::InBag(0), nanommo::InBag(1)));
  EXPECT_TRUE(predicted.ahead());
}

TEST(PredictedBag, TakesOneOffAStackThatsUsed) {
  const nanommo::ItemTable items = Items();
  nanommo::PredictedBag predicted(items);
  nanommo::protocol::Inventory server;
  (*server.mutable_bag())[2] = Of(kPotion, 3);
  nanommo::protocol::ItemAction use;
  use.set_use(2);
  predicted.Act(server, use);
  EXPECT_EQ(predicted.Shown(server, 0.0f).bag().at(2).count(), 2u);
}
