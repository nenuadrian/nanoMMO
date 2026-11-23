# 46. The servers send their sentences as their English and args, which the client translates

## Status

Accepted

## Context

Episode 95 translated the client's own words ([0045](0045-translate-the-client-with-gettext-po-files-at-display-and-keep-the-content-english.md))
and left the servers' sentences English: a login server's errors, such as a name that's taken, and
what a world says in the chat, its refusals, such as a bag that's full, a trade's, a duel's and a
war's news, a guild's, what a monster dropped, a quest's progress and prizes, and a boss's fall,
and those of the offline world, which runs the servers' rules. There were over a hundred, each
glued together in English on the server, from names and numbers, with "a" or "an" before an item,
an s for more than one, and "The " before a monster or an NPC, and sent as a line's text. Over a
hundred of the servers' tests read that English. The NPC who tells a story was named in English
over it too, and the lands' English names are keys.

A world has players in any language at once, and doesn't know which each is in. Episode 96 had to
settle what a server sends in place of its English, what a sentence's id is, how a name, a number
and the content's names go into it, how plurals and lists work, how the template gets the servers'
sentences, and what becomes of the tests, the logs, and a client of another version, which reads
the login server's English to hear which version to get.

## Decision

- A server's sentence goes as a `Message` (`protocol/nanommo.proto`): its English, with `$0` to
  `$9`, which is its id, as a client string's English is its key, a plural's English for more than
  one, with its count, and its args. An arg is a number, a name a player gave or a version, which
  stays as it is, a sentence of its own inside the other, or the content's, by its id: an item, or
  so many of one, a kind of monster, or so many, an NPC, a quest, a skill or a slot of the bag.
- The client translates it as it shows it (`Translated`, in `client/messages.h`): the English
  through `Tr`, or `TrN` for a plural, the content's names through `NameOf`, in their contexts,
  such as `item 8`, and the args filled in where the translation puts them. So many of an item is a
  plural of the item's own, "a Bone" and "$0 Bones", in its context.
- On the servers, `Msg` and `MsgN` make a `Sentence`, the message and its English, from a part for
  each placeholder: a number, a `Name`, `Of` the content, or another sentence
  (`protocol/messages.h`). They mark the English for the template, as `N_` does the client's, and
  `scripts/po.py extract` finds them in `protocol/` and `server/`, with a `// Translators:` comment
  above for what goes in.
- A refusal is a `Refusal`: an `absl::Status` whose message is the English, with the `Message` as
  its payload, which `SentenceOf` takes out, so the world's, the bag's and the guilds' rules return
  statuses as before, and their tests read the English as before. A `Notice` has both too.
- A list is a sentence of "$0 and $1", with "$0, $1" before the rest of it.
- `Chat` has `message`, 4, in place of a server's `text`, and `npc`, 5, for an NPC who speaks,
  which the chat names. The stories are mvGPT's, and stay English.
- The login server's `LoginReply` has `why`, 7, the `Message`, beside `error`, its English.
- The chat's commands stay as they are, in every language, and so do the names players give.
- The offline world runs the servers' rules, so it says the same sentences, the same way.

## Alternatives considered

- Keys, such as `trade.asked`, for ids. Each sentence would need its English somewhere else, for
  English, for the servers' logs and for their tests, and an untranslated one would show its key.
  English as the key is [0045](0045-translate-the-client-with-gettext-po-files-at-display-and-keep-the-content-english.md)'s
  choice, and one that isn't translated shows in English.
- A number for each sentence, an enum in the protocol. A line would be smaller, but each sentence
  would change the protocol, and the client would need a table of the English to say any. The
  English is about as long as the line it replaces, on TCP, a few lines a second at most.
- Translating on the servers, in a language the client sends as it logs in. Each world would read
  every language's file, the servers would keep each player's language, and the chat would stay in
  the one they logged in with when they changed it in the settings, where the client's own words
  change at once.
- Sending the English beside the message, for the logs. The servers keep it beside the message, in
  a `Sentence`, a `Notice` and a status, so it needn't cross the wire, but for a login reply's
  `error`, which older clients read.
- The content's names as text, "Bone" in its context, `item 8`, for the client to look up. The
  servers would have to know the client's contexts, and send what the client's content has already,
  which `ClientHash` makes sure of.
- `std::expected` with a `Message` in place of `absl::Status` (not weighed in an episode). Each
  function that refuses would change, with its callers and its tests, where a status's payload
  changes none of them.
- Articles in each language: "a" or "an" before an item and "The" before a monster, as the English
  had. Romanian puts its article after the noun, as "Scheletul", in a form that depends on the
  noun's gender, which the content doesn't say. So many of an item is a plural of each item's own,
  which a translator writes, and a name that would need an article goes in quotes, or after a word
  that takes none, as "De la $0:", as the client's own sentences do.
- ICU's MessageFormat, with plurals and selects inside each sentence. It's as large for the servers'
  sentences as [0045](0045-translate-the-client-with-gettext-po-files-at-display-and-keep-the-content-english.md)
  found it for the client's, and PO files' plurals do what they need.

## Consequences

- The template has 156 sentences from the servers, three of which the client has too, such as "$0
  gold", and a plural for each of the 13 items, 512 strings in all, and Romanian has all of them. A
  new sentence on a server needs `Msg` or `MsgN`, and the template brought up to date, or CI's lint
  fails, as for a string of the client's.
- A sentence's English is its id, so changing a word in it on a server is a new string, which
  `scripts/po.py update` offers the old translation for, fuzzy, as for the client's.
- The client needs the content to name what a sentence names, which `ClientHash` makes sure it
  has. Something it hasn't got shows as "something".
- 0.96.0 changes what the client and the servers say to each other, so the login server turns away
  a 0.95.0 client, in English, as `error` has it. A 0.96.0 client shows the English of a login
  server before 0.96.0, which turns it away the same way.
- Romanian doesn't know a player's gender, so a sentence with a player as its object, such as "$0
  l-a ucis pe $1.", takes the masculine.
- The quest window's "$0 experience" is a plural now, as the servers' is in a quest's prizes, as
  Romanian counts experience in points, "20 de puncte de experiență".
- The chat translates each of its hundred lines as it draws them, a few lookups each, as the rest
  of the interface does.
