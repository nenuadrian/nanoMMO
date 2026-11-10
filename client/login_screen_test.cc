#include "client/login_screen.h"

#include <cstdint>
#include <vector>

#include "content/items.h"
#include "gtest/gtest.h"

namespace {

// A sword and a shirt worn from the start, a second sword, which doesn't fit
// the hand the first holds, and potions, which nobody wears.
nanommo::ItemTable Kit() {
  return *nanommo::ParseItems(R"pb(
    starting { item: 1 }
    starting { item: 2 }
    starting { item: 4 }
    starting { item: 3 count: 3 }
    items {
      id: 1
      weapon { reach: 1.6 swing_ms: 2000 hit_ms: 900 damage: 15 }
      equip { slot: SLOT_MAIN_HAND }
    }
    items {
      id: 2
      equip { slot: SLOT_BODY }
    }
    items {
      id: 3
      stack: 10
      use { heal: 40 cooldown_ms: 10000 }
    }
    items {
      id: 4
      weapon { reach: 1.8 swing_ms: 2000 hit_ms: 900 damage: 20 }
      equip { slot: SLOT_MAIN_HAND }
    }
  )pb");
}

}  // namespace

TEST(LoginScreen, DrawsANewCharacterInTheKitItllStartWith) {
  nanommo::protocol::CharacterListing fresh;
  fresh.set_id(7);
  fresh.set_fresh(true);
  const nanommo::Figure figure = nanommo::LoginScreen::FigureOf(fresh, Kit());
  EXPECT_EQ(figure.id, 7u);
  // The first sword in the hand, the shirt on, and nothing of the rest.
  EXPECT_EQ(figure.weapon, 1u);
  EXPECT_EQ(figure.wearing, std::vector<uint32_t>{2});
  EXPECT_EQ(figure.to, nanommo::protocol::MOTION_IDLE);
}

TEST(LoginScreen, DrawsACharacterThatsPlayedInWhatItWears) {
  nanommo::protocol::CharacterListing played;
  played.set_id(8);
  played.set_weapon(4);
  played.add_wearing(2);
  const nanommo::Figure figure = nanommo::LoginScreen::FigureOf(played, Kit());
  EXPECT_EQ(figure.weapon, 4u);
  EXPECT_EQ(figure.wearing, std::vector<uint32_t>{2});
}
