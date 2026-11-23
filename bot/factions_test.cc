#include "bot/factions.h"

#include <cstdint>
#include <map>
#include <optional>

#include "content/content.h"
#include "gtest/gtest.h"

namespace {

using nanommo::Errand;

// The game's: 1 Concordia, 2 Fortuna and 3 Victoria, whose heralds are kinds
// 7, 8 and 9.
constexpr uint32_t kConcordia = 1;
constexpr uint32_t kFortuna = 2;
constexpr uint32_t kFortunasHerald = 8;
// Lands, by their places in kLands.
constexpr int kMeadows = 0;
constexpr int kAshlands = 1;

nanommo::Content Content() { return *nanommo::ReadContent(NANOMMO_CONTENT); }

}  // namespace

TEST(Factions, ACrowdOfBotsSwearsToEachInTurn) {
  const nanommo::Content content = Content();
  std::map<uint32_t, int> sworn;
  for (uint32_t seed = 0; seed < 300; ++seed) {
    ++sworn[nanommo::FactionFor(content, seed)];
  }
  EXPECT_EQ(sworn, (std::map<uint32_t, int>{{1, 100}, {2, 100}, {3, 100}}));
  // Without any, there's none to swear to.
  EXPECT_EQ(nanommo::FactionFor(nanommo::Content(), 7), 0u);
}

TEST(Factions, GoesToItsFactionsHeraldOnceItSeesThem) {
  // Fortuna's herald, with an id of their own, and nobody else's.
  nanommo::protocol::World seen;
  nanommo::protocol::Npc* herald = seen.add_npcs();
  herald->set_id(9);
  herald->set_kind(kFortunasHerald);
  herald->set_x(-8.5f);
  herald->set_z(1.75f);

  const std::optional<Errand> oath =
      nanommo::Oath(Content(), kFortuna, seen, kMeadows);
  ASSERT_TRUE(oath);
  EXPECT_EQ(oath->kind, Errand::Kind::kSwear);
  EXPECT_EQ(oath->herald, 9u);
  EXPECT_FLOAT_EQ(oath->x, -8.5f);
  EXPECT_FLOAT_EQ(oath->z, 1.75f);
  // There's no faction 9.
  EXPECT_FALSE(nanommo::Oath(Content(), 9, seen, kMeadows));
}

TEST(Factions, GoesToWhereAHeraldStandsBeforeItSeesThem) {
  // Concordia's herald isn't in sight, so the bot goes to where they stand
  // in the Meadows, with nobody to swear at yet.
  const std::optional<Errand> oath =
      nanommo::Oath(Content(), kConcordia, {}, kMeadows);
  ASSERT_TRUE(oath);
  EXPECT_EQ(oath->kind, Errand::Kind::kSwear);
  EXPECT_EQ(oath->herald, 0u);
  EXPECT_FLOAT_EQ(oath->x, -8.0f);
  EXPECT_FLOAT_EQ(oath->z, -1.0f);
  // The Ashlands have no heralds.
  EXPECT_FALSE(nanommo::Oath(Content(), kConcordia, {}, kAshlands));
}

TEST(Factions, KnowsOnceItsSworn) {
  nanommo::protocol::World seen;
  seen.set_you(3);
  nanommo::protocol::Player* you = seen.add_players();
  you->set_id(3);
  nanommo::protocol::Player* other = seen.add_players();
  other->set_id(4);
  other->set_faction(kFortuna);
  // Another player's faction isn't the bot's.
  EXPECT_FALSE(nanommo::Sworn(seen));
  you->set_faction(kFortuna);
  EXPECT_TRUE(nanommo::Sworn(seen));
}
