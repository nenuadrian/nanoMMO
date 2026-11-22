# Translations

The client speaks English and Romanian. The settings' Interface page, on O, has Language, which
lists each language by its own name, English, Română and Pseudo, and the interface changes as soon
as one's picked. The client keeps it in `settings.txtpb` as `Interface`'s `language`, 6, a code:
none for English, `ro` for Romanian, and `pseudo`.

What's translated is the client's own words, each window's and each button's, and the content's
names, as they're shown: the items, the skills, the monsters and their skills, the NPCs, the quests
and what their givers say, the recipes, the ways to wear the hair, the builds and the lands. The
server's sentences stay English for now (see [What stays English](#what-stays-english)).

## How it works

The translations are gettext's PO files, in `po/`: `nanommo.pot`, the template, with each string
in English, and a file for each language, `ro.po`, with what each is in Romanian. The build copies
the folder next to the client, as it does the fonts, and the web build preloads it into the page
(see [The client in a browser](web.md)). The client reads a language's file with a reader of its
own, `Catalog` in `client/translations.h`: contexts, plurals, strings over several lines, escapes
and comments. It leaves out what's marked fuzzy or left untranslated, and those show in English.

The code is in English, and asks for each string in the player's language as it shows it:

| Call | What for | English, untranslated |
|---|---|---|
| `Tr("Sound")` | A string | `Sound` |
| `Tr("compass", "N")` | A string in a context, for one that could mean two things | `N` |
| `TrN("$0 gold", "$0 gold", n)` | The form of a plural for a count | The first for 1, the second for any other count |
| `TrLabel("Mute")` | An ImGui label, which ImGui takes as the widget's ID too | `Mute` |
| `N_("Walk")` | Marks a string in a table for the template, which `Tr` translates where it's shown | `Walk` |

- A sentence with something in it, a name or a number, has placeholders, `$0` to `$9`, which
  `absl::Substitute` fills in: `absl::Substitute(Tr("$0 asks you to join $1."), from, guild)`. A
  translation can put them in any order. The client leaves out a translation with a placeholder its
  English doesn't have, which `absl::Substitute` would have nothing to fill in with, and shows the
  English.
- A plural's forms are the language's, by its header's `Plural-Forms`: English has two, one for 1
  and another for the rest, and Romanian three, "1 galben", "3 galbeni" and "20 de galbeni", by
  `nplurals=3; plural=(n==1 ? 0 : (n==0 || (n%100 > 0 && n%100 < 20)) ? 1 : 2);`. `PluralRule`
  reads the C expression, with `?:`, `||`, `&&`, the comparisons, `%`, numbers and brackets, and
  checks that it picks one of the forms for every count.
- The game's own helpers translate what they're given, so their callers pass English:
  `GameButton`'s label, `BeginPanel`'s title, the help's lines, the bottom right's buttons'
  tooltips, and the labels over the steppers and swatches that make a character.
- A widget keeps its English label as its ID: `TrLabel("Mute")` is `Fără sunet###Mute` in
  Romanian, and a panel and a button draw their translated words while ImGui knows them by their
  English. So changing the language moves no window, and no two widgets with one translation share
  an ID.
- The content's names are English in the content files, as `ClientHash` hashes them and the
  servers say them, so each is translated as it's shown, in a context that says what it names:
  its kind and its id, `item 8`, `skill 3`, `monster 4`, `monster 4 skill 1`, `npc 2`, `quest 1`
  and `recipe 3`, or, for those without an id, its kind alone, `hair`, `build` and `land`
  (`client/names.h`). Translating them changes nothing that two sides have to agree on. "3
  Skeletons", for a quest, is a plural of its own, as the English has it, the name with an s.
- Cinzel, the titles' and the buttons' face, has no ș, ț or ă, so the client merges Alegreya Sans
  Bold into it, which ImGui reaches for any letter Cinzel hasn't got (`LoadFonts`, in
  `client/theme.cc`).

## The workflow

`scripts/po.py` keeps the files, with Python 3 and nothing else:

```bash
# The template, from the client's sources and the content.
scripts/po.py extract
# Each language's file, with the template's strings merged into it.
scripts/po.py update
# What's out of date or wrong, as CI checks it.
scripts/po.py check
```

- `extract` finds each string the client's sources hand to `Tr`, `TrN`, `TrLabel` or `N_`, or to
  one of the helpers that translate, and the content's names, and writes `po/nanommo.pot`, the same
  way each time, with where each is from. A comment above a call, starting `// Translators:`, goes
  with its string, for whoever translates it.
- `update` merges the template into each `po/*.po`, as gettext's `msgmerge` would: a translation
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
4. Check the fonts have its letters: Alegreya Sans has Latin's, Greek's and Cyrillic's, and Cinzel
   gets what it lacks from Alegreya Sans. A language in another script needs a font that has it,
   merged in, in `LoadFonts`.

## Pseudo

Pseudo is a language made from the template, with no file of its own: each string's letters get a
mark, the ones Romanian has where there's one, Ș, ț and ă, and é, ñ and the like elsewhere, and it
grows by a third, in brackets, "Settings" as "[Șéțțîñgș ~~~]". On the screen it shows three things
at once: a string that isn't marked for translation, which stays plain English, one that a longer
language would cut off, and a letter a font can't draw. It's for whoever adds a string, and
`Pseudo` in `client/translations.h` makes it.

## What stays English

- The server's sentences: a login server's error, such as a name that's taken, and what a world
  says in the chat, as a duel ends, a guild's news or a refusal, such as a bag that's full, and
  those of the offline world, which runs the server's rules. They're made on the server, in
  English, glued together from names and numbers, and episode 96 sends them as message ids with
  their arguments instead, which the client translates.
- The storyteller's stories, which mvGPT tells in English, and so the words it's given.
- The details of a connection that fails, such as the system's, after "Couldn't reach the server:".
- The chat's commands, `/g`, `/trade`, `/duel` and `/war`, and the keys' letters.
- Names that players give: characters, guilds and worlds.

Romanian doesn't address the player, so it picks neither "tu" nor "dumneavoastră": a button is a
noun, such as "Ștergere", and a sentence impersonal, with "se". It has ș and ț with their commas
below. Why translations are PO files read by the client, and why the content stays English, is
[decision 45](decisions/0045-translate-the-client-with-gettext-po-files-at-display-and-keep-the-content-english.md).
