#ifndef NANOMMO_PROTOCOL_MESSAGES_H_
#define NANOMMO_PROTOCOL_MESSAGES_H_

#include <concepts>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "content/content.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// The text with each placeholder, $0 to $9, filled in with its arg, as
/// absl::Substitute does, and $$ as a dollar. A placeholder without an arg
/// comes out as nothing, as a sentence from a server can have any number.
std::string Fill(std::string_view text, std::span<const std::string> args);

/// What goes into one of a server's sentences: the arg, which the client
/// shows in its player's language, and in English, for the servers' logs and
/// tests.
struct Part {
  protocol::Arg arg;
  std::string english;
};

/// A server's sentence: the message the client says in its player's
/// language, and in English.
struct Sentence {
  protocol::Message message;
  std::string english;
};

/// A number, as it is.
Part Number(uint64_t number);

/// A name a player gave, a character's, a guild's or a world's, or a
/// version, which stays as it is in every language.
Part Name(std::string_view name);

/// An item, by its id: its name alone, "Bone", or, with a count, so many of
/// it, "a Bone", "an Ash Blade" or "3 Bones".
Part Of(const items::Item& item, uint32_t count = 0);

/// So many of the item by its id, or "something" for one the table hasn't
/// got.
Part Of(const ItemTable& items, uint32_t item, uint32_t count);

/// A kind of monster: its name alone, "Skeleton", or so many of it, "3
/// Skeletons".
Part Of(const monsters::Kind& kind, uint32_t count = 0);

/// The rest of the content, by its id, and a slot of the bag, which have
/// their names alone.
Part Of(const npcs::Kind& kind);
Part Of(const quests::Quest& quest);
Part Of(const skills::Skill& skill);
Part Of(const factions::Faction& faction);
Part Of(items::Slot slot);

/// A sentence inside another, which is translated on its own.
Part Of(Sentence phrase);

/// The two forms in English of so many of an item or of a kind of monster, a
/// plural's, for one and for more, which the client's template has too: "a
/// Bone" and "$0 Bones", and "$0 Skeleton" and "$0 Skeletons".
std::pair<std::string, std::string> Counted(const items::Item& item);
std::pair<std::string, std::string> Counted(const monsters::Kind& kind);

namespace messages_internal {

inline Part ToPart(Part part) { return part; }
inline Part ToPart(Sentence phrase) { return Of(std::move(phrase)); }
inline Part ToPart(std::integral auto number) {
  return Number(static_cast<uint64_t>(number));
}

Sentence Make(std::string_view one, std::string_view many, uint64_t count,
              std::vector<Part> parts);

}  // namespace messages_internal

/// A server's sentence, by its English, with a part for each of its
/// placeholders, in order: a number, a Name, the content's Of, or a sentence
/// of its own. It marks the English for the template too, as N_ marks the
/// client's.
template <typename... Parts>
Sentence Msg(std::string_view text, Parts... parts) {
  return messages_internal::Make(
      text, "", 0, {messages_internal::ToPart(std::move(parts))...});
}

/// A plural: its English for one, and for any other count, which picks the
/// form, in the client's language as TrN does.
template <typename... Parts>
Sentence MsgN(std::string_view one, std::string_view many, uint64_t count,
              Parts... parts) {
  return messages_internal::Make(
      one, many, count, {messages_internal::ToPart(std::move(parts))...});
}

/// A status that says why something was refused: in English as its message,
/// and as a sentence, its payload, for the client to say in its player's
/// language.
absl::Status Refusal(absl::StatusCode code, const Sentence& why);

/// What a status says: a Refusal's sentence, or, from a status that isn't
/// one, its message as it is.
Sentence SentenceOf(const absl::Status& status);

/// A message as a client shows it in English, with the content's names from
/// the content: what the servers' tests read of what a player hears.
std::string English(const protocol::Message& message, const Content& content);

}  // namespace nanommo

#endif  // NANOMMO_PROTOCOL_MESSAGES_H_
