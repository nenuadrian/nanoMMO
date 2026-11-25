# 44. Looks are chosen once, as places in the content's lists, and names show over whoever's pointed at or picked

## Status

Accepted

## Context

Until episode 94, nobody chose how their character looked. Their id did. `FeaturesOf`, in
`content/figures.h`, scatters the id's bits and picks a skin, a hair colour and a way to wear the
hair from the figures file's lists with them, and the client picked one of the rig's builds by the
id too, so every client drew a player the same, with nothing to send or save. Making a character
was a name, typed under the list of characters, and Make them. Most MMOs have a screen to make a
character on, where a player chooses how they'll look, and episode 94 adds one. It had to settle
what a look is, in the messages and in MySQL, and who checks it, as the login server, which makes
the characters, reads no content, and can't tell how many builds or hairs there are. And what
becomes of the characters there are, whose ids have picked their looks since they were made.

The screen had filled up too. Every name within 50 metres was over its head, a player's or a
monster's with its level and its health, and a player's guild, and an NPC's on its own, which in a
crowd hid the land, and the fight in it, under the names. The game's name was over the worlds and the
characters, a plate with the land's name over the minimap, and the windows had lines a player reads
once and then has on the screen for good. They were the bag's count of its slots used and how to
throw an item away, and the map's line on clicking to walk.

## Decision

- A character's looks are a `Looks`, in `protocol/nanommo.proto`, with `build`, a place in the rig's
  builds, and `skin`, `hair_color` and `hair`, places in the figures file's lists. A `Build` and a
  `Hair` have a `name` for the screen, "Slight" and "Broad", and "Short" to "Bald", and the skins
  and the hair colours are swatches.
- A player chooses them once, as they make the character (`LoginScreen::BuildCreate`), on a screen
  that starts with looks picked at random, and has Random to pick again. They go with the name, in
  `LoginRequest`'s `looks`, and nothing changes them after.
- The login server puts them into `players`, in `build`, `skin`, `hair_color` and `hair`, from V16,
  each a `TINYINT UNSIGNED NULL`, in the statement that makes the character (`Characters::Create`).
  It keeps any place that fits its column, up to 255 (`Characters::kMostLook`), and refuses one
  past that, saying "That isn't a look there is."
- Each client wraps each place round to what its lists have (`FeaturesOf` in `client/animator.h`,
  and `Characters::For` for the build). The figures and the rig are in `ClientHash`, so every
  client in a world has the same lists, and a place out of range is just another look, the same on
  every screen.
- The looks are in MySQL alone. The world server reads them with the character's row as it loads
  them (`Players::Load`), and sends them in `Player`'s `looks`, once a second, with the name, and
  the login server lists them with each character, in `CharacterListing`'s `looks`. The tick's save
  and the saver leave them out.
- A character from before 0.94.0 has NULL in all four, and no looks go with it, so its id picks
  its looks, as before, and nobody wakes up as someone else. The bots and the red-team client make
  their characters without looks, and theirs are picked the same way.
- A name shows over a head while the pointer's on them (`PickAnyone`, by `Who`), or they're picked,
  and otherwise only as the settings say (`ShowsName` in `client/nameplates.h`). The settings'
  Interface page has "Names over heads", to show them always over monsters, other players, NPCs or
  you, each off to begin with, and with a name its health, its level and a guild, each on, kept as
  `Interface.nameplates` in `settings.txtpb`. An NPC with a quest for you shows its mark without its
  name.
- The hints leave the screen for a help window, on H (`client/help_window.h`), with a page each for
  moving, fighting, items, quests, guilds and duels, chat, and the keys, which are the settings'
  list (`BuildKeys`). The bag loses its count of slots used, and the map its line on clicking to
  walk. The game's name is over the login alone, the minimap loses its plate with the land's name,
  and "Show the minimap" (`Interface.minimap`) can take the minimap off the screen. O opens the
  settings, and a round button in the bottom right opens the help.

## Alternatives considered

- Keeping the looks the id picks. There's nothing to send or save, but nobody chooses, and two
  players who'd like to look alike, or apart, can't.
- The login server checking each place against the content's lists. It reads no content, and each
  world can run content of its own, with a `ClientHash` of its own. The world server has the
  content, and could check as a character enters, but a look it found out of range would have to
  become one there is, which wrapping does anyway, the same on every client.
- Colours as values, the skin and the hair colour as 0xRRGGBBAA, as the figures file has them (not
  weighed in an episode). A player could send any colour at all, and refusing the ones that aren't
  in the lists needs the lists, which the login server doesn't have.
- Keeping the looks in the player's hash in Redis, with the rest of what the tick saves. They never
  change, so each save would carry four numbers for nothing, and the saver would write them again.
  MySQL's row, which the world server reads as a character enters anyway, has them.
- Filling in the older characters' columns in V16, with the looks their ids pick. That's
  `FeaturesOf`'s scattering of the id over how many of each the lists have, which a migration, in
  SQL, doesn't know. NULL leaves the pick to the code that has it.
- Changing the looks after a character's made (left out of episode 94). It's a place to do it and
  a rule for how often, where choosing once needs neither.
- Every name, always, as before. In a crowd, the names hid the land, and the fight in it.
- No names over heads at all, and the target's frame alone (not weighed in an episode). Telling
  who's who would take a click, and a click on a player who isn't an opponent or an enemy picks
  nobody, where pointing costs nothing.
- An NPC's quest mark hidden with its name. A player looking for who has a quest for them would
  have to point at each NPC, so the mark shows anyway.
- Keeping the hints where they were. A player reads each once, and then has it on the screen for
  good. In the help, each is a key away.

## Consequences

- A player chooses from 360 looks, before what they wear, with two builds, six skins, six ways to
  wear the hair and five hair colours. A new character starts at random among them, rather than on
  the first of each.
- A look is a place, so a new build, skin, hair colour or way to wear the hair goes at the end of
  its list. One put in before the end, or taken out, changes the looks of each character who chose
  one after it, and nothing in the content's checks catches that.
- An older character's looks are their id's, by how many of each the lists have, so a new entry in
  a list, at its end or not, changes most of theirs.
- The figures and the rig are in `ClientHash`, so a new look needs a new client, as a new item
  does.
- A client that sends a place out of range gets a look there is, which everyone sees the same, so
  there's nothing in it to cheat with. Past 255, the login server refuses it.
- The looks cost each world a few bytes for each player in each client's world, once a second,
  with the name, and nothing in the worlds between.
- Nobody can change their looks once they're made. A player who wants others makes another
  character, of the four a world has room for.
- In a crowd, nothing's over anyone's head until the pointer's on them. Finding a friend by name
  takes pointing at them, or Other players on.
- An enemy at war has a red name only while they're pointed at or picked, unless Other players is
  on, so a war's enemies are harder to see coming.
- A monster's skill on its way has its bar and its name under the monster's health only while that
  monster's name shows, so one that's neither picked nor pointed at shows its skill by the shape on
  the ground alone.
- Your own name doesn't show, unless You is on.
- The land's name is the loading screen's as you arrive, and the map's title, and on the screen
  nowhere else.
- The help is words in `client/help_window.cc`, which a change to how something plays has to change
  too, as the content can't. The keys are one list, for the help and the settings.
- Nothing points a new player to the help but its button in the bottom right, with H in its
  tooltip.
- A settings file from before has no `nameplates` and no `minimap`, so it gets the defaults, which
  are names only for whoever's noticed, and the minimap.
