#include "client/trade_window.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"

TEST(TradeWindow, AsksForAnOfferAgainInTheOrderOfTheBag) {
  nanommo::protocol::TradeSide side;
  (*side.mutable_bag())[5].set_item(3);
  (*side.mutable_bag())[2].set_item(4);
  side.set_gold(30);
  side.set_ready(true);

  const nanommo::protocol::TradeOffer offer = nanommo::OfferOf(side);
  EXPECT_EQ(std::vector<uint32_t>(offer.bag().begin(), offer.bag().end()),
            (std::vector<uint32_t>{2, 5}));
  EXPECT_EQ(offer.gold(), 30u);
}
