# 45. The client translates with gettext's PO files, as it shows each string, and the content stays English

## Status

Accepted

## Context

Until episode 95, every word on the screen was English, written into the client, in about
340 strings, from the menu's and the help's to the login's, some of them sentences glued together
from pieces with `absl::StrCat`, some printf's, such as "%s asks you to join %s.", and plurals made
by putting an s on the end, as in "3 Skeletons". The content's names, the items', the monsters',
the quests' and the rest, came from the content files, which `ClientHash` hashes so that a client
and a world agree on what they draw, and the lands' English names are keys, which the servers and
`kLands` look lands up by. Cinzel, the face of the titles and the buttons, has no ș, ț or ă.

Episode 95 puts the client in a second language, Romanian. It had to settle where translations
live and in what format, how the code asks for one, how a sentence with a name or a number in it
is translated, when a language's word order isn't English's, how plurals work in a language with
three forms, and what becomes of the content's names. The server's sentences, a login server's
error, or what a world says in the chat, are English glued together on the server, and episode
96 is theirs.

## Decision

- Translations are gettext's PO files, in `po/`, which holds `nanommo.pot`, the template, and a file
  for each language, `ro.po` for Romanian. The client reads them with a reader of its own
  (`Catalog`, in `client/translations.h`), with contexts, plurals, strings over several lines,
  escapes and comments, and leaves out what's fuzzy or untranslated. The build copies `po/` next to
  the client, and the web build preloads it into the page.
- The code is English, and English is each string's key. The code uses `Tr("Sound")`,
  `Tr("compass", "N")` in a context, `TrN(one, many, n)` for a plural, `TrLabel` for an ImGui
  label, and `N_` to mark a string in a table, which is translated where it's shown. What has no
  translation shows in English. The game's helpers, `GameButton`, `BeginPanel`, the help's lines
  and the bottom right's tooltips, translate what they're given.
- A sentence with something in it has placeholders, `$0` to `$9`, which `absl::Substitute` fills
  in, in the order the translation puts them. The client leaves out a translation with a
  placeholder its English doesn't have, which `absl::Substitute` couldn't fill in, and shows the
  English.
- A plural takes its forms from the language's `Plural-Forms`, a C expression of n, which
  `PluralRule` reads and works out, with `?:`, `||`, `&&`, the comparisons, `%`, numbers and
  brackets.
- The content stays English. Each name is translated as it's shown, in a context that says what it
  names, by its kind and id, such as `item 8` (`client/names.h`), so the content files, `ClientHash`
  and what the client and the servers say to each other don't change with the language.
- A widget's ID is its English label, after `###`, so that a window stays where it is, and two
  widgets with one translation stay two, in any language.
- The settings' Interface page has Language, kept as `Interface.language`, 6, a code, which the
  client puts into use as soon as it's picked. Pseudo is a language made from the template, for
  finding what isn't translated or doesn't fit.
- Alegreya Sans Bold is merged into Cinzel, for the letters Cinzel hasn't got.
- `scripts/po.py` writes the template from the sources and the content (`extract`), merges it into
  each language (`update`), and checks both in CI's lint stage (`check`).
- Romanian doesn't address the player, so it picks neither "tu" nor "dumneavoastră". A button is a
  noun, and a sentence impersonal, with "se". It writes ș and ț with their commas below.

## Alternatives considered

- GNU gettext itself, with libintl and `.mo` files compiled from the PO files. It's a library
  for each platform and the web build, a compile step for each language, and a locale picked by the
  environment, where a game picks its language in its settings. Reading the PO files, which are
  text, takes the client a few hundred lines, and the files are the same ones.
- gettext's own tools, `xgettext` and `msgmerge`, for the template and the merge. They don't know
  our helpers, which take English, or the content files, and they aren't in the lint stage's
  Python image. `scripts/po.py` does both in a file, with nothing to install.
- JSON files, a key and its string each (not weighed in an episode). There's no plural, no
  context, no fuzzy and no translator's comment, and no editor that knows them, so each would be
  ours to add.
- ICU's MessageFormat, with plurals and choices inside each message. ICU is a large library, with
  tens of megabytes of data, which the web build would carry too, for plurals that the
  Plural-Forms rule does in one line.
- Mozilla's Fluent. A translator can say more in it than in a PO file, as each language's grammar
  needs, but its libraries are Rust's, JavaScript's and Python's, and few editors know it.
- Keys, such as `settings.sound`, in place of the English. The code would read less plainly, and
  English would need a file of its own, where now an untranslated string shows its English.
- Translating the content files, a name for each language in each entry. `ClientHash` would hash
  the language, and a client in Romanian couldn't play in a world with one in English, and the
  lands' names are keys.
- printf's `%s` in the translations. A translation can't put its pieces in another order, and a
  `%` in a translation would read as a format.
- A title face with Romanian's letters in place of Cinzel. The game would look different in every
  language, where merging changes only the letters Cinzel lacks.
- Translating the server's sentences in this episode. They're glued together from names and
  numbers on the server, which doesn't know each player's language, and episode 96 sends them as
  message ids with their arguments, for the client to translate.

## Consequences

- Each string on the screen has to go through `Tr` or a helper that translates, and the template
  through `scripts/po.py extract` and `update`, or CI's lint fails. Pseudo shows one that was
  missed, as plain English among the marked.
- A new string shows in English until it's translated, and a changed one too, while its old
  translation is fuzzy, so a language can fall behind without anything breaking.
- A content name that's renamed keeps its context, so `update` offers its old translation, fuzzy.
  "3 Skeletons" is a plural of each kind of monster a quest asks for, which the template makes as
  the English does, with an s.
- Each frame looks up a few hundred strings in a `std::map`, and the content's names build their
  context to look up by. A translation's pointer lasts until the language changes, so nothing
  keeps one from a frame to the next.
- Romanian's ș, ț and ă in a title or a button are Alegreya Sans's, which is plainer than
  Cinzel's capitals around them.
- The server's sentences, the storyteller's stories and a failed connection's details are English
  in a Romanian client until episode 96, and the chat mixes the two.
- The settings are in the world, so the login and the screens before it are in the language last
  picked, English on a first run.
- Pseudo is in the settings' list for everyone, as the one way to see it, which a player may pick
  and find odd.
