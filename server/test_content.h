#ifndef NANOMMO_SERVER_TEST_CONTENT_H_
#define NANOMMO_SERVER_TEST_CONTENT_H_

#include <memory>
#include <utility>

#include "content/content.h"
#include "content/items.h"
#include "content/monsters.h"
#include "content/skills.h"

namespace nanommo::testing {

// The tests' own content, so that the game's can change.

inline ItemTable Sword() {
  return *ParseItems(R"pb(
    starting { item: 1 }
    items {
      id: 1
      weapon { reach: 1.6 swing_ms: 2000 hit_ms: 900 damage: 15 }
      equip { slot: SLOT_MAIN_HAND }
    }
  )pb");
}

inline SkillTable Kick() {
  return *ParseSkills(R"pb(
    skills {
      id: 1
      name: "Kick"
      level: 2
      cooldown_ms: 6000
      effect { cast_ms: 1000 hit_ms: 500 reach: 1.4 damage: 10 push: 3 }
    }
  )pb");
}

// What a player might carry: what stacks, what's worn, and what rolls.
inline ItemTable Gear() {
  return *ParseItems(R"pb(
    starting { item: 1 }
    starting { item: 2 count: 3 }
    items {
      id: 1
      name: "Sword"
      weapon { reach: 1.6 swing_ms: 2000 hit_ms: 900 damage: 15 }
      equip { slot: SLOT_MAIN_HAND }
    }
    items {
      id: 2
      name: "Potion"
      stack: 10
      use { heal: 40 cooldown_ms: 10000 }
    }
    items { id: 3 name: "Bone" stack: 50 }
    items {
      id: 4
      name: "Cap"
      equip {
        slot: SLOT_HEAD
        modifiers { stat: STAT_ARMOUR amount: 2 }
        modifiers { stat: STAT_HEALTH amount: 10 }
      }
    }
    items {
      id: 5
      name: "Blade"
      weapon { reach: 1.6 swing_ms: 2000 hit_ms: 900 damage: 20 }
      equip {
        slot: SLOT_MAIN_HAND
        modifiers { stat: STAT_DAMAGE amount: 1 }
      }
      rolls { stat: STAT_DAMAGE min: 2 max: 4 }
    }
  )pb");
}

/// Content with these items, skills and monsters, and nothing else, for a
/// World.
inline std::shared_ptr<const Content> With(ItemTable items,
                                           SkillTable skills = {},
                                           MonsterTable monsters = {}) {
  return std::make_shared<const Content>(
      Content{.items = std::move(items),
              .monsters = std::move(monsters),
              .skills = std::move(skills)});
}

inline monsters::Kind Skeleton() {
  return ParseMonsters(R"pb(
           kinds {
             id: 1
             name: "Skeleton"
             level: 1
             max_health: 50
             experience: 50
             fight {
               damage: 5
               swing_ms: 900
               hit_ms: 650
               reach: 1.2
               sight: 3
               leash: 4
               wander: 3
             }
           }
         )pb")
      ->kinds.at(1);
}

}  // namespace nanommo::testing

#endif  // NANOMMO_SERVER_TEST_CONTENT_H_
