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
    starting_weapon: 1
    items {
      id: 1
      weapon { reach: 1.6 swing_ms: 2000 hit_ms: 900 damage: 15 }
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

/// Content with these items and skills, and nothing else, for a World.
inline std::shared_ptr<const Content> With(ItemTable items,
                                           SkillTable skills = {}) {
  return std::make_shared<const Content>(
      Content{.items = std::move(items), .skills = std::move(skills)});
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
