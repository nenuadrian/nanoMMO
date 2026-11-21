#include "client/chat_window.h"

#include <string_view>

#include "gtest/gtest.h"

TEST(ChatWindow, SendsALineThatStartsWithSlashGToTheGuild) {
  EXPECT_EQ(nanommo::GuildLine("/g meet at the gate"), "meet at the gate");
  EXPECT_EQ(nanommo::GuildLine("meet at the gate"), std::nullopt);
  // Nothing for the guild is still for the guild, not the world.
  EXPECT_EQ(nanommo::GuildLine("/g "), "");
  EXPECT_EQ(nanommo::GuildLine("/go away"), std::nullopt);
}

TEST(ChatWindow, AsksToTradeWithThePlayerNamedAfterSlashTrade) {
  EXPECT_EQ(nanommo::TradeName("/trade bob"), "bob");
  EXPECT_EQ(nanommo::TradeName("trade bob"), std::nullopt);
  EXPECT_EQ(nanommo::TradeName("/trader"), std::nullopt);
}

TEST(ChatWindow, AsksTheGuildNamedAfterSlashWarToWar) {
  EXPECT_EQ(nanommo::WarName("/war Stone Bears"), "Stone Bears");
  EXPECT_EQ(nanommo::WarName("war Stone Bears"), std::nullopt);
  EXPECT_EQ(nanommo::WarName("/warlock"), std::nullopt);
}

TEST(ChatWindow, AsksToDuelOrAnswersAfterSlashDuel) {
  const auto duel = [](std::string_view typed) {
    return nanommo::DuelLine(typed).value_or(nanommo::protocol::DuelAction());
  };
  EXPECT_EQ(duel("/duel bob").ask(), "bob");
  EXPECT_TRUE(duel("/duel yes").accept());
  EXPECT_TRUE(duel("/duel no").decline());
  EXPECT_TRUE(duel("/duel yield").yield());
  EXPECT_FALSE(nanommo::DuelLine("duel bob").has_value());
  EXPECT_FALSE(nanommo::DuelLine("/duelling").has_value());
}
