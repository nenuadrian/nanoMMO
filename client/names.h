#ifndef NANOMMO_CLIENT_NAMES_H_
#define NANOMMO_CLIENT_NAMES_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "content/content.h"

namespace nanommo {

/// The content's names, and its quests' words, in the language in use. The
/// content keeps them in English, as ClientHash hashes them and the servers
/// say them, so each is translated as it's shown, in a context that says what
/// it names: by its kind and its id, such as "item 8", or, for those that
/// have no id, its kind alone, such as "hair".
const char* NameOf(const items::Item& item);
const char* NameOf(const skills::Skill& skill);
const char* NameOf(const monsters::Kind& kind);
const char* NameOf(const npcs::Kind& kind);
const char* NameOf(const quests::Quest& quest);
const char* NameOf(const recipes::Recipe& recipe);
const char* NameOf(const factions::Faction& faction);
const char* NameOf(const figures::Hair& hair);
const char* NameOf(const rig::Build& build);

/// A monster's skill, which is numbered among its kind's: "monster 4 skill 1".
const char* NameOf(uint32_t kind, const monsters::Skill& skill);

/// A slot of the bag, which the content's SlotName says in English, as the
/// servers do.
const char* NameOf(items::Slot slot);

/// What a quest's giver says, giving it.
const char* TextOf(const quests::Quest& quest);

/// So many of a kind of monster, "3 Skeletons", which the template has as its
/// name after the count, with an s for more than one, as the English does.
std::string NameOf(const monsters::Kind& kind, uint32_t count);

/// So many of an item, "a Bone" or "3 Bones", which the template has as a
/// plural of its own, as the English does.
std::string NameOf(const items::Item& item, uint32_t count);

/// A land's name, by its English one, which the servers and kLands use: one
/// this client doesn't know stays as it is.
std::string LandName(std::string_view land);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_NAMES_H_
