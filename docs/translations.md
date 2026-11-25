# Translations

The client speaks English and Romanian. The settings' Interface page, on O, has Language, which
lists each language by its own name, English, Română and Pseudo, and the interface changes as soon
as one's picked. The client keeps it in `settings.txtpb` as `Interface`'s `language`, 6, a code that's
none for English, `ro` for Romanian, and `pseudo`.

What's translated is the client's own words, each window's and each button's, the content's names,
as they're shown, and what the servers say. The content's names cover the items, the skills, the
monsters and their skills, the NPCs, the quests and what their givers say, the recipes, the
factions, the ways to wear the hair, the builds and the lands. What the servers say is a login
server's errors, such as a name that's taken, and a world's lines in the chat, such as a bag that's
full, a guild's news or what a monster dropped, which come as messages the client translates (see
[The servers' sentences](#the-servers-sentences)). What stays English is [below](#what-stays-english).

## How it works

The translations are gettext's PO files, in `po/`. There's `nanommo.pot`, the template, with each
string in English, and a file for each language, `ro.po`, with what each is in Romanian. The build
copies the folder next to the client, as it does the fonts, and the web build preloads it into the
page (see [The client in a browser](web.md)). The client reads a language's file with a reader of its
own, `Catalog` (`client/translations.h`), which reads contexts, plurals, strings over several
lines, escapes and comments. It leaves out what's marked fuzzy or left untranslated, and those show
in English.

The code is in English, and asks for each string in the player's language as it shows it, with the
calls below.

| Call | What for | English, untranslated |
|---|---|---|
| `Tr("Sound")` | A string | `Sound` |
| `Tr("compass", "N")` | A string in a context, for one that could mean two things | `N` |
| `TrN("$0 gold", "$0 gold", n)` | The form of a plural for a count | The first for 1, the second for any other count |
| `TrLabel("Mute")` | An ImGui label, which ImGui takes as the widget's ID too | `Mute` |
| `N_("Walk")` | Marks a string in a table for the template, which `Tr` translates where it's shown | `Walk` |

- A sentence with something in it, a name or a number, has placeholders, `$0` to `$9`, which
  `absl::Substitute` fills in, as in `absl::Substitute(Tr("$0 asks you to join $1."), from, guild)`. A
  translation can put them in any order. The client leaves out a translation with a placeholder its
  English doesn't have, which `absl::Substitute` would have nothing to fill in with, and shows the
  English.
- A plural's forms are the language's, by its header's `Plural-Forms`. English has two, one for 1
  and another for the rest, and Romanian three, "1 galben", "3 galbeni" and "20 de galbeni", by
  `nplurals=3; plural=(n==1 ? 0 : (n==0 || (n%100 > 0 && n%100 < 20)) ? 1 : 2);`. `PluralRule`
  reads the C expression, with `?:`, `||`, `&&`, the comparisons, `%`, numbers and brackets, and
  checks that it picks one of the forms for every count.
- The game's own helpers translate what they're given, so their callers pass English. That goes for
  `GameButton`'s label, `BeginPanel`'s title, the help's lines, the bottom right's buttons'
  tooltips, and the labels over the steppers and swatches that make a character.
- A widget keeps its English label as its ID, so `TrLabel("Mute")` is `Fără sunet###Mute` in
  Romanian, and a panel and a button draw their translated words while ImGui knows them by their
  English. So changing the language moves no window, and no two widgets with one translation share
  an ID.
- The content's names are English in the content files, as `ClientHash` hashes them and the
  servers say them, so each is translated as it's shown, in a context that says what it names, which
  is its kind and its id, `item 8`, `skill 3`, `monster 4`, `monster 4 skill 1`, `npc 2`, `quest 1`,
  `recipe 3` and `faction 1`, or, for those without an id, its kind alone, `hair`, `build` and `land`
  (`client/names.h`). Translating them changes nothing that two sides have to agree on. "3
  Skeletons", for a quest, is a plural of its own, as the English has it, the name with an s.
- Cinzel, the titles' and the buttons' face, has no ș, ț or ă, so the client merges Alegreya Sans
  Bold into it, which ImGui reaches for any letter Cinzel hasn't got (`LoadFonts`, in
  `client/theme.cc`).

## The servers' sentences

The servers don't know each player's language, and a world has players in every one at once, so
what they say goes to the client as a `Message` (`protocol/nanommo.proto`). A message has its
English, with `$0` to `$9`, which is its key, as a client string's English is, and what goes in each
placeholder, its args. The client translates it as the chat draws it, with `Translated`
(`client/messages.h`), and the login screen as the reply comes.

| Arg | What | In the player's language |
|---|---|---|
| `number` | A number | As it is |
| `name` | A name a player gave, a character's, a guild's or a world's, or a version | As it is, in every language |
| `phrase` | A sentence of its own inside the other, such as so much gold, or the rest of a list | Translated on its own |
| `item`, `monster`, `npc`, `quest`, `skill`, `slot`, `faction` | The content's, by its id, or a slot of the bag | `NameOf`, in its context, such as `item 8` |
| `count`, with an `item` or a `monster` | So many of it | A plural of each item's own, "a Bone" and "$0 Bones", or of the monster's, "$0 Skeletons" |

- On the servers, `Msg` makes a sentence, and `MsgN` a plural, from its English and a part for each
  placeholder, whether a number, a `Name`, `Of` the content, or another sentence
  (`protocol/messages.h`), as in
  `Msg("Your bag is full. Make room for $0 first.", Of(items, cap, 1))`. Each has its English too,
  "a Cap" or "3 Bones" as before, which the servers' tests read, and `English` says a message again
  from the content's names, as a test hears it from a server.
- A refusal is a `Refusal`, an `absl::Status` whose message is the English, with the sentence as
  its payload, which `SentenceOf` takes back out, so the world's rules, the bag's and the guilds'
  return statuses as they did. A guild's action that MySQL fails, whose status isn't one, is "That
  didn't work. Try again." to the player, and in the logs as it is.
- A plural has its English for one and for more, and a count, which picks its form in the
  player's language, as `TrN` does, such as "$0 gold", or "a minute" and "$0 minutes".
- A list, such as what a monster dropped, "2 Bones, a Sword and 5 gold", is a sentence of "$0 and
  $1", with "$0, $1" before the rest of it, so each language joins it its own way.
- So many of an item has a plural of the item's own, in its context, "a Bone" and "$0 Bones", which
  `scripts/po.py` makes from the items file as `Counted` makes them on the servers, so a language
  says one or 20 of each its own way, with its own gender.
- A login server's reply has `why`, the sentence, beside `error`, its English, which a client from
  before 0.96.0 shows. A client of another version reads `error` to hear which version to get
  (see [Versions](releases.md#versions)), so it stays.
- An NPC who speaks, such as a storyteller, comes with its kind, `Chat`'s `npc`, and the chat names
  it in the player's language, over a story that's English.
- The offline world runs the servers' rules, so it says the same sentences, which the client
  translates the same way.

## The workflow

`scripts/po.py` keeps the files, with Python 3 and nothing else.

```bash
# The template, from the client's sources and the content.
scripts/po.py extract
# Each language's file, with the template's strings merged into it.
scripts/po.py update
# What's out of date or wrong, as CI checks it.
scripts/po.py check
```

- `extract` finds each string the client's sources hand to `Tr`, `TrN`, `TrLabel` or `N_`, or to
  one of the helpers that translate, each sentence the servers make with `Msg` or `MsgN`, in
  `protocol/` and `server/`, and the content's names, with a plural for each item, and writes
  `po/nanommo.pot`, the same way each time, with where each is from. A comment above a call,
  starting `// Translators:`, goes with its string, for whoever translates it, and a server's says
  what goes in its placeholders. A sentence the servers and the client both have, such as "$0
  gold", is one string.
- `update` merges the template into each `po/*.po`, as gettext's `msgmerge` would. A translation
  stays with its string, a string that's changed a little gets the old translation, marked fuzzy
  for someone to look at, and one that's gone stays at the end, obsolete, for a string that comes
  back.
- Then the new strings are translated in the PO file, in an editor, or in a PO editor such as
  Poedit, which shows the fuzzy ones too. A fuzzy one shows in English until its mark is taken off.
- `check` runs in CI's lint stage (see [Code style and checks](checks.md)), and fails on a
  template that isn't what `extract` would write, a PO file that isn't merged with it, one that
  can't be read, a plural rule that can't be followed, a plural without each of its language's
  forms, a translation whose placeholders aren't its English's, one with `##` in it, where ImGui
  would cut it off, and a letter a language shouldn't have, such as ş for ș in Romanian. A plural's
  forms can leave out the count, as Romanian's for one does, but between them they have the
  English's placeholders. It prints how many each language has translated.
- The client's tests read each file in `po/` with the client's own reader too, so that one the
  check reads, the client can.

## Another language

1. Copy `po/nanommo.pot` to `po/<code>.po`, with the language's code, such as `fr`, and in its
   header, `Language:` the code, and `Plural-Forms:` the language's rule, from
   [gettext's list](https://www.gnu.org/software/gettext/manual/html_node/Plural-forms.html).
2. Translate it, and run `scripts/po.py update` and `scripts/po.py check`.
3. Add it to `kLanguages`, in `client/translations.h`, with its name in itself.
4. Check the fonts have its letters. Alegreya Sans has Latin's, Greek's and Cyrillic's, and Cinzel
   gets what it lacks from Alegreya Sans. A language in another script needs a font that has it,
   merged in, in `LoadFonts`.

## Pseudo

Pseudo is a language made from the template, with no file of its own. Each string's letters get a
mark, the ones Romanian has where there's one, Ș, ț and ă, and é, ñ and the like elsewhere, and it
grows by a third, in brackets, "Settings" as "[Șéțțîñgș ~~~]". On the screen it shows three things
at once. They are a string that isn't marked for translation, which stays plain English, one that a
longer language would cut off, and a letter a font can't draw. It's for whoever adds a string, and
`Pseudo` in `client/translations.h` makes it.

## What stays English

- The storyteller's stories, which mvGPT tells in English, and so the words it's given. The
  storyteller's name, over them, is in the player's language.
- The details of a connection that fails, such as the system's, after `Couldn't reach the server:`.
- The chat's commands, `/g`, `/trade`, `/duel` and `/war`, the `yes` after `/duel`, and the keys'
  letters, in the sentences that say to type them too.
- Names that players give to characters, guilds and worlds.
- The servers' logs, which have each sentence's English, and a login server's `error`, the English
  for a client from before 0.96.0, which is how a 0.95.0 client hears it's turned away.

Romanian doesn't address the player, so it picks neither "tu" nor "dumneavoastră". A button is a
noun, such as "Ștergere" or "Înrolare", and a sentence impersonal, with "se", or a noun, such as
"Ieșire din breasla $0." An item's or a skill's name that would need an article goes in quotes,
„$0”, and so many of an item takes "de" from 20 on, "20 de Oase". The factions' names are Latin,
and stay as they are, after "facțiunea" where Romanian would decline them, as in "Intrare în facțiunea
Concordia." It has ș and ț with their commas below. Why
translations are PO files read by the client, and why the content stays English, is
[decision 45](decisions/0045-translate-the-client-with-gettext-po-files-at-display-and-keep-the-content-english.md),
and why the servers send their sentences as their English and args is
[decision 46](decisions/0046-the-servers-send-their-sentences-as-their-english-and-args-for-the-client-to-translate.md).
