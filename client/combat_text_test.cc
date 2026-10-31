#include "client/combat_text.h"

#include <cstdint>

#include "gtest/gtest.h"
#include "protocol/nanommo.pb.h"

namespace {

nanommo::protocol::Event Happened(nanommo::protocol::EventKind kind,
                                  uint32_t player, uint32_t amount,
                                  uint32_t monster = 0) {
  nanommo::protocol::Event event;
  event.set_kind(kind);
  event.set_player(player);
  event.set_amount(amount);
  event.set_monster(monster);
  return event;
}

}  // namespace

TEST(CombatText, ShowsEachEventOverItsUnitUntilItFades) {
  nanommo::CombatText text;
  // We're player one. We hit monster seven, player two drinks a potion, and
  // we reach level five.
  text.Add(Happened(nanommo::protocol::EVENT_HIT, 1, 15, 7), 1);
  text.Add(Happened(nanommo::protocol::EVENT_HEAL, 2, 40), 1);
  text.Add(Happened(nanommo::protocol::EVENT_LEVEL, 1, 5), 1);
  ASSERT_EQ(text.popups().size(), 3u);
  const nanommo::Popup& hit = text.popups()[0];
  EXPECT_EQ(hit.text, "15");
  EXPECT_EQ(hit.unit, 7u);
  EXPECT_TRUE(hit.monster);
  const nanommo::Popup& heal = text.popups()[1];
  EXPECT_EQ(heal.text, "+40");
  EXPECT_EQ(heal.unit, 2u);
  EXPECT_FALSE(heal.monster);
  EXPECT_LT(heal.scale, hit.scale);
  EXPECT_EQ(text.popups()[2].text, "Level 5");

  // A kind from a newer server shows nothing.
  text.Add(Happened(static_cast<nanommo::protocol::EventKind>(9), 1, 3), 1);
  EXPECT_EQ(text.popups().size(), 3u);

  // They stay for their life, then go.
  text.Update(nanommo::CombatText::kLife / 2);
  EXPECT_EQ(text.popups().size(), 3u);
  text.Update(nanommo::CombatText::kLife / 2);
  EXPECT_TRUE(text.popups().empty());
}
