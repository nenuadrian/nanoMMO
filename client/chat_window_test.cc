#include "client/chat_window.h"

#include "gtest/gtest.h"

TEST(ChatWindow, SendsALineThatStartsWithSlashGToTheGuild) {
  EXPECT_EQ(nanommo::GuildLine("/g meet at the gate"), "meet at the gate");
  EXPECT_EQ(nanommo::GuildLine("meet at the gate"), std::nullopt);
  // Nothing for the guild is still for the guild, not the world.
  EXPECT_EQ(nanommo::GuildLine("/g "), "");
  EXPECT_EQ(nanommo::GuildLine("/go away"), std::nullopt);
}
