# Gameplay

## Making a character

Once you've logged in, you pick a world, and then one of your characters in it to enter as. A
world where you have nobody yet goes straight to making someone, and New character, under your
characters, does too, while you have fewer than four there, as many as the login server lets an
account have in a world (see [Characters](accounts.md#characters)). The game's name is over the
login alone, so the worlds and the characters have the screen.

You name them, and choose how they look, on the right, while they stand on the stage in the
middle, in the kit they'll start with, turning as you drag them and coming closer with the mouse
wheel:

| Choice | From | Picked with |
|---|---|---|
| Build | Slight or Broad, the rig's builds | The arrows either side of its name, which go round |
| Skin | Six skins, from pale to dark, from the figures file | A swatch each |
| Hair | Short, Long, Tied back, Short with a beard, Bald with a beard, or Bald | The arrows |
| Hair colour | Black, brown, red, fair or grey | A swatch each |
| Shirt | Orange, blue, green, purple, red or yellow, the players' colours, which the Linen Shirt is dyed in | A swatch each |

- The screen starts with looks picked at random, so that new players don't all start out alike,
  and Random picks again. Each change shows on the stage at once.
- Create, or Enter in the name, makes them, and you're back at your characters, with them picked.
  A name that's taken, or breaks the rules, gets a line saying why, under the buttons, and the
  screen keeps what you chose.
  «  Characters goes back without making anyone.
- The looks are yours for good: nothing in the game changes them. Everyone sees you so, as each
  world sends your looks with your name.
- A character made before 0.94.0 has no looks of their own, and looks as their id picks, as
  everyone did before, so nobody wakes up as someone else. So do the bots' characters, which they
  make without looks. One made with 0.94.0 chose the rest, but not their shirt, and their id picks
  its colour.

The names of the builds and of the ways to wear the hair are in the content, as `Build.name` in
`content/rig.txtpb` and `Hair.name` in `content/figures.txtpb` (see [Content](content.md)), and a
look is a place in those lists
([decision 44](decisions/0044-looks-are-chosen-once-and-names-show-over-whoever-is-pointed-at-or-picked.md)).

## Monsters, health and levels

The monsters are Quaternius's skeleton, from [Poly Pizza](https://poly.pizza/m/wODZYCgX5Z). It's
public domain (CC0), so it's in the repo, in `assets/monsters`, as it was downloaded:
[assets/monsters/README.md](../assets/monsters/README.md) says where it's from. The client loads it as
it is, for each kind of monster whose look names it. It scales it to the kind's height, finds its clips by the word in their names, and walks with its
run, played at 0.4 of its speed, which puts the feet at the walking speed. It reads the bind pose
from the file's inverse bind matrices, as raylib binds the mesh to where the bones rest, and the
skeleton's arms rest about 70 degrees from where its mesh was bound.

The server spawns the monsters that `content/monsters.txtpb` lists (see [Content](content.md)): three monsters
around the middle of each land, and up to three more at each of its camps, forty in the Meadows and
the Ashlands and twenty-four in the Crypt, which the land's seed places between the beach and the
rock, at least thirty metres from the middle. The camps are darts thrown at the land: a place too
near a camp already there (`kCampSpacing`, thirty metres) is thrown again, like one in the sea, so
they spread over the land evenly, where places picked at random bunch up and leave gaps. It runs their AI each
tick: a monster rests for two to six seconds, walks to a random place within its kind's wander of where
it spawned, at 1.8 metres a second, and rests again. If the sea or a tree stops it, it rests, and
picks another place. The world's random generator takes a seed, so the tests pick theirs.

## Fighting

Click a monster to pick it: it gets a red ring at its feet, a frame beside yours with its name,
level and health, and its name over its head for as long as it's picked (see
[Names over heads](#names-over-heads)). A click on the ground lets it go, and so does Escape, its
death, or going into another land. The cursor is a crosshair over a living monster, a hand over an
NPC within talking reach, and the arrow elsewhere. Each input carries the monster you picked
(`Input.target`), which the world keeps as your target.

Space uses Strike, the skill every player has: a swing of your weapon at the monster you picked,
which turns you to face it. Out of its reach, it first runs you up to it, to most of the reach
(`World::kCloseIn`, 0.8), so that a monster that steps back is still within it; the keys call that
off. With nothing picked, you swing at the air, the way you face. Strike's effect in the skills file
is `weapon: true`, so its reach, its damage and its timing are the weapon's in your hand, and the
sword's numbers are in the items file: it reaches 1.6 metres, a whole swing takes two seconds and
hits 0.9 seconds in, as it comes across the front, for fifteen health, and each level after the
first adds five to whatever you hold, and what you wear adds its damage. You stand still until a
swing's over. Strike is a quick swing, with a `pace` of 0.55: it takes that share of the weapon's
swing, 1.1 seconds, lands that much sooner, and does that share of its damage, if the monster is
still alive and in reach. Strike goes on into Return Cut, as quick, a cut back the other way, and
that into Overhead Cut, at 0.9 of the swing, which pushes harder: Space within 0.6 seconds of one
ending (its `next` and `window_ms`) uses the next, and after that, or after any other skill, Strike
again. Holding Space down goes round the three, and they hurt as much over a second as the whole
swing would. With no weapon in your main hand, Space does nothing, and putting the weapon away ends
a swing. All of it happens on the server: the client only says that Space is down, in `Input`, and
the world uses the skill the skills file names as its `attack`, or the next in its chain, as if its
key had gone down.

A monster goes for the nearest living player within its kind's sight, three metres for a skeleton, if that player is within
its leash of the monster's home, four metres. The content's checks keep each fixed home further than that from the middle of the
island, so the middle, where players start and come back, is safe. A monster that's hurt goes for
whoever's hurt it most instead, seen or not, while they're within its leash, so that a kick that
knocks it out of sight doesn't make it forget them: each keeps a threat table, of the health each
player's blows have taken from it (`Monster.threat`), and on a tie it keeps to the one it's
fighting. A monster runs at them at the
players' running speed, and swings once it's within 1.2 metres. The skeleton's swing takes 0.9 seconds, its attack animation, and hits for five,
0.65 seconds in, less the armour you wear, down to one. An Ash Skeleton, in the Ashlands, is level two, and hits for eight. Run, or die, and it gives up and wanders back. Ash and Crypt Skeletons have a skill each too (see [Monsters' skills](#monsters-skills)).

A monster with no health left dies, and whoever killed it gets its experience, fifty for a skeleton, and ninety for an Ash Skeleton,
and what it drops, into their bag (see [Items](#items)). A boss's death is shared (see
[The world boss](#the-world-boss)).
Going up a level fills your health, to the new level's most. A player with no health left dies. The
dead play their death animation and lie there for ten seconds (`World::kRespawnTime`), or a
monster as long as its kind's `respawn_ms`. Then a monster comes back where it spawned, at full
health. A player can come back once the wait is over,
and stays down until they ask to: the world refuses before then. While you're dead, the world
darkens, red comes in from the sides, and under "You died" a button counts down the seconds the
server says you have left to Respawn, which brings you back in the middle of your land, at full health. The screen goes
dark as you come back, and the world fades in, so you don't see yourself jump there. Each world a
dead player gets says how long they have left to wait (`respawn`, in milliseconds), and the bots
come back as soon as they can.

Players and monsters have a level and health, which everyone sees. A player's experience is its own:
the server works out its level, and sends its experience into the level, and what the next level
takes, to its client alone, in the message with its id that goes after the world, or with each part
over UDP. Each level takes a hundred more experience than the one before, and a player has a hundred
health at level one, and twenty more for each level after it, and what they wear adds its health.

Experience and health are saved with where you are (see [Saving players](saving.md)), so you
come back with the health you left with. If you leave dead, you come back as the respawn would bring
you back, in the middle at full health. A player saved before health was kept joins at full health.

### Monsters' skills

The Ash Skeleton and the Crypt Skeleton have a skill each, a big blow they telegraph: its shape
shows on the ground first, and fills, for long enough to step out of, and then it lands on whoever's
still in it. The plain skeleton only swings, so that new players learn that first. Each kind's
`skills` are in the monsters file (see [Content](content.md)).

| Monster | Skill | Shape | Shows for | Damage | Cooldown | Starts within |
|---|---|---|---|---|---|---|
| Ash Skeleton | Ground Slam | A circle three metres round it | 2 s | 30 | 10 s | 1.5 metres |
| Crypt Skeleton | Bone Sweep | A cone 90 degrees wide, four metres out in front of it | 1.5 s | 35 | 8 s | 2.5 metres |

- A monster uses a skill in place of a swing, once it's ready and the player it's fighting is
  within the skill's range: the first of its kind's skills that is, in the monsters file's order,
  and for a boss, the first its phase uses.
  It stands where it is, facing the player, while the shape shows, and then the skill lands on each
  living player in the shape, less the armour they wear, down to one, as a swing does. Then it
  fights on. Its cooldown starts as it starts, and runs down whatever the monster does.
- The shape stays where and how the monster aimed it as it started (`World::LandSkill`, and
  `InShape` in `content/monsters.h`), so a player who steps out is out, wherever the monster goes. A
  push doesn't stop a skill, or move its shape, so a kick that knocks the monster away doesn't save
  whoever stays where they were. A throw calls it off, as it does a swing.
- Your own swing or skill holds you where you are until it's over, Strike for 1.1 seconds, so let
  go of Space as a shape shows, and step out as your swing ends.
- The world checks the shape against where each player is on the server as it lands. Your keys take
  half a round trip to get there, and your screen hears that it's landed half a round trip after it
  has, so you have to be out a round trip before the fill ends on your screen
  ([decision 35](decisions/0035-a-monsters-skill-lands-where-it-was-aimed.md)).
- While a monster's using a skill, the world sends which, by its id among its kind's, and how long
  until it lands, in milliseconds, as `Monster.skill` and `Monster.lands_in`, so it's sent again
  each tick, as a monster that moves is.
- The client draws the shape on the ground where the monster stood as it started, the way it faced
  (`client/telegraphs.h`), in the skill's colour: faint all over, with its edges strong, and filling
  from the monster out until it lands, counting down by itself between worlds. Under the monster's
  health, while its name shows over it (see [Names over heads](#names-over-heads)), a thinner bar
  fills with it, with the skill's name. Both go once it lands, or the monster dies.
- The game's bots run out of a shape they're in, straight away from the monster, to a metre past
  its end, or out of a cone by its side, where that's nearer, as it is in a narrow one (`StepOut`,
  in `bot/wander.h`), and then think again.

## The world boss

The Bone Colossus stands in the Meadows, 45 metres from the middle, on open ground past the gate
to the Ashlands, at 8, -44, with room round it for a crowd: the skeleton, four and a half metres
tall, in the colour of old bone. It's level 10, with 6000 health, and swings for 18 from three
metres, every 1.6 seconds. It sees players eight metres off, and its leash is twenty metres, five
times a skeleton's, so that a crowd has room round it to fight, and to step out of its skills. A
kind with `phases` in the monsters file is a boss (see [Content](content.md)).

It fights in three phases, by its health, each with skills of its own, which it tries in the
monsters file's order, the Bone Storm first:

| Phase | Its health | Its skills | It calls up |
|---|---|---|---|
| 1 | 100% to 60% | Colossal Slam, Grave Sweep | |
| 2 | 60% to 30% | and Bone Lance | 3 Skeletons |
| 3 | 30% down | and Bone Storm | 3 Ash Skeletons |

| Skill | Shape | Shows for | Damage | Cooldown | Starts within |
|---|---|---|---|---|---|
| Bone Storm | A circle nine metres round it | 3.5 s | 60 | 18 s | 9 metres |
| Colossal Slam | A circle five metres round it | 2 s | 40 | 9 s | 3 metres |
| Grave Sweep | A cone 120 degrees wide, seven metres out in front of it | 1.8 s | 35 | 7 s | 4 metres |
| Bone Lance | A cone 30 degrees wide, fourteen metres out in front of it | 2.2 s | 50 | 12 s | 14 metres |

- A blow that takes it past a phase's share of its health takes it into that phase, and one that
  takes it past two, into the second (`World::Hurt`).
- It goes for whoever's taken the most of its health, wherever they stand within its leash, as
  every monster does now, and not for whoever hit it last: in a crowd, a boss that turned to each
  new blow would face nobody for long
  ([decision 36](decisions/0036-a-boss-fights-who-hurt-it-most-and-shares-its-death.md)).
- Once nobody who's hurt it is alive and within its leash, it goes home to start again: back at
  full health and in its first phase, it runs there, and nothing hurts it on its way, so that
  nobody wears it down from past its leash, and a fight that's lost leaves it as it was for the
  next.
- As each phase starts, its adds come round it, three metres out (`World::kAddsAround`), once the
  blows of that tick have landed, so that the blow that called them doesn't hit them too. An add
  fights where its boss does, for whoever it sees there, as far as the boss sees and goes. It
  doesn't come back to life: it falls with its boss, and goes once the boss is back in its first
  phase, at home or alive again.
- Its death is everyone's who took at least 5% of its health (`World::kEnoughHelp`), 300 of the
  Colossus's 6000, whoever landed the last blow: each gets its 800 experience, a roll of its loot
  of their own, into their bag, and a kill for their quests, those who died in the fight too. A
  player who took less gets nothing.
- Everyone in its land hears it fall, in a line from nobody, which the chat shows in bold, in a
  title's light gold (`CHANNEL_LAND`): "The Bone Colossus has fallen to alice and 7 others. It rises
  again in 2 minutes.", naming whoever took the most. It lies dead for two minutes, its kind's
  `respawn_ms`, and thirty seconds before it's back (`World::kRisesSoon`) the land hears "The Bone
  Colossus rises again in 30 seconds.", and then "The Bone Colossus has risen."
- While you've picked it, or it's within thirty metres of you (`kBossBarReach`), its bar is across
  the top of the screen, under the target's frame (`client/boss_bar.h`): its name, its health, a
  mark where each phase after the first starts, and the phase it's in, which each world sends as
  `Monster.phase`. Its name is over its head while the pointer's on it, or it's picked, as any
  monster's is, and a click anywhere on it picks it: a monster's box for the mouse is as tall as
  its kind, and as much wider.
- The game's bots go for a boss within sixty metres of them (`kJoinTheFight`, in `bot/quests.h`),
  as the Colossus is of the middle of the Meadows, before anything else, and back to it after each
  skill they run out of, and after dying, so the bots that fill a world take it on together.

## Skills

A skill is a move with a cooldown. Each player knows Strike and the swings it goes on into, on
Space, and the thrust from the start, and learns the others from a trainer (see
[Learning skills](#learning-skills)). For now, while we try them out, each of the sixteen in
`content/skills.txtpb` is from level one:

| Skill | From level | Cooldown | What it does |
|---|---|---|---|
| Strike | 1 | none | 0.55 of the weapon's damage to the monster you picked, within its reach |
| Return Cut | 1 | none | After Strike, on Space: the same, back the other way |
| Overhead Cut | 1 | none | After Return Cut, on Space: 0.9 of the weapon's damage, and knocks it 0.8 metres back |
| Thrust | 1 | 8 s | 35 damage to the monster you picked, within 2.4 metres |
| Kick | 1 | 6 s | 10 damage to the monster you picked, within 1.4 metres, and knocks it 3 metres back |
| Second Wind | 1 | 30 s | Heals 40, up to your most |
| Whirlwind | 1 | 12 s | 25 damage to each monster within 2.5 metres |
| Earthsplitter | 1 | 15 s | 40 damage to each monster within 3.5 metres, and knocks them 2 metres back |
| Frost Nova | 1 | 18 s | 30 damage to each monster within 4 metres |
| Searing Arc | 1 | 14 s | 35 damage to each monster within 7 metres, in a slice 100 degrees wide in front of you |
| Stormcaller | 1 | 20 s | 60 damage to the monster you picked, within 10 metres |
| Phantom Rush | 1 | 14 s | Dashes 7.5 metres through the monster you picked, and 55 damage a moment later |
| Skyrend | 1 | 18 s | 70 damage over three blows to each monster in a slice 140 degrees wide, 3 metres out: throws them up, cuts across, and dashes them down |
| Tempest Flurry | 1 | 16 s | 72 damage over nine blows to the monster you picked, within 2.4 metres, the last throwing it up |
| Cyclone | 1 | 15 s | 60 damage over four blows to each monster within 2.6 metres, spinning 5 metres forward |
| Severing Moon | 1 | 22 s | 95 damage to each monster in a slice 160 degrees wide, 4.5 metres out, knocking them 2.2 metres back |
Each level after the first adds five to a skill's damage, as it does to a weapon's. There's no
skill tree: a trainer teaches each of its skills to any player who has reached its level.

The hotbar along the bottom of the screen has Strike first, on Space, then the others in the order
of their levels, but for the swings Strike goes on into, which are Space's too, each on a number key, 1 for the first, up to 9, then a row above it, on Shift and the same keys,
for nine more, and a click on one uses it too.
Drag one onto another to swap them: the client keeps the order in its settings, and the settings'
Interface page puts the levels' order back. Each shows its icon, in the colour of its first fx, and
its key, and its name and level when the mouse is over it. A skill you don't
know yet is greyed out, with the level it needs, or Learn once you've reached it, and one that's
cooling down is shaded, a slice that shrinks clockwise from the top, with the seconds left in the
middle. The client works that out from the milliseconds the server sends and the cooldown in the
skills file.

The server decides it all. The client sends a skill's id in `Input` once, as its key goes down, and
the world keeps it until the player is free: a skill asked for during another starts as that ends.
Then it starts if the player knows it and its cooldown is over, and otherwise it goes. A skill that
hits one monster needs a living one picked, and goes without one, keeping its cooldown; the hotbar
shows it red then. Strike is the one that doesn't, and swings at the air. Out of its reach, a skill
runs the player up to the monster first, and starts once they're there. A skill holds the player for
its cast time, and lands partway through, or at each of its `blows`, sharing its damage out between
them, as Whirlwind does each time its blade comes round in front: on the monster the player picked if
it's still in reach, or on each one in reach. It starts its cooldown as it starts. Each blow knocks
what it lands on away from the player by its push, from a quarter of a metre for a swing to three for
the kick, over 0.2 seconds (`kPushTime`), fast at first and slowing, where the sea or a tree lets it,
while the monster does nothing else, though a swing it started goes on. A push that comes during
another starts again from where the monster is.

A blow can throw a monster up too, to its `lift` in metres off the ground, and the server flies it
under a gravity of 14 metres a second each second (`kGravity`), a bit more than the Earth's so that
it doesn't float, while it does nothing else; thrown, it drops a swing it was in. A blow on a monster
in the air sends it up a little again (`kPop`), and a lift below nothing dashes it down. However many
players take turns at it, a monster is in the air for two seconds at most (`kMostInAir`), and once
it's down it can't be thrown again for two more (`kSteady`), so that it gets to fight back. Each
monster's height goes to the clients, in `Monster.height`, while it's in the air.

A skill's `strides` carry the player as it goes: each is how far along the way they faced as it
started, which aiming at the monster turned them to, they've gone by a time into it, eased from the
stride before as a move's keys are. The server walks them there each tick, so that everyone sees the
dash, and like a walk it slides along the sea or a tree, or stops. Searing Arc's effect has an `arc`, in degrees, and hits only the monsters within that slice in
front of the player, half of it to either side of the way they face. Cooldowns run down whatever the player
does, dead or alive, and aren't saved: a player who leaves and comes back finds their skills ready.

A player using a skill has the motion `MOTION_SKILL`, its id, the monster they picked
(`Player.target`), and how many skills they've started (`Player.casts`), so that everyone's
clients can show it, its fx land on the same monster on every screen, and a swing straight after a
swing starts its move again. A skill's look in the skills file has its move and its fx, which the client draws (see
[Moves and fx](graphics.md#moves-and-fx)). Your cooldowns are yours alone, so they go with your
experience, after the world: the milliseconds left for each skill that isn't ready, by its id.

### Learning skills

A trainer, an NPC whose kind has `teaches` in the content, teaches a player a skill it teaches,
once they've reached the skill's level and stand within four metres of it, as close as you'd stand
to talk to someone. `World::Learn` checks each of those, and the skill isn't one the player knows
already, or says why not, in words the server can send the player. The world joins each player
with the starting skills, whatever was saved, so the database doesn't need to know which they are.

Click a trainer, and a window lists the skills it teaches, each with its icon and name, and Known,
the level it's from, or a Learn button. The client asks for a skill in `Input`'s `learn`, with the
trainer's id and the skill's, and the server sends a refusal back as a line in the chat, from
nobody, like the storyteller's. The window stays open, and its button turns into Known once the
server has taught you the skill. The skills a
player knows go to them with their cooldowns, after the world, by id.

## Items

Each player carries a bag of 24 slots, and wears an item in each of six slots: the main hand, the
off hand, the head, the body, the legs and the feet. A new player starts with the kit from the items
file (see [Content](content.md)), the sword in hand, a linen shirt, cloth trousers and plain shoes on,
and three health potions in the bag. They get it once: a trip through a gate doesn't give them
another, and neither does emptying the bag, so a player from before the clothes has none.

What you wear adds to you: its damage to each blow you land, with a weapon or a skill, its health to
the most you can have, and its armour comes off each blow a monster lands, down to one. The weapon in
your main hand is the one you swing. The server adds it all up as what you wear changes, not as you
fight. Some items roll modifiers of their own as they're made, such as an Ash Blade's damage, which
stay with that one.

Press I, or the bag's button in the bottom right, to open the bag. What you wear is in the character window (C). Each
slot shows its item's icon, in the item's tint, and how many are in it, and over it a tooltip says
what it is, where it's worn, what it adds, what it rolled, what using one does and how many stack.
Drag an item onto another slot, in either window, to move it, stack it, swap it, wear it or take it
off, with Shift for half the stack. It follows the mouse, and each slot under it lights green if the
server would take it there and red if not: the client tries the move on a copy of the bag with the
server's own rules (`server/inventory.h`, a library both link). Let go of it over no window, and the
bag asks whether to throw it away. Double-click it to drink it, wear it or take it off, and
right-click it for a menu that throws it away too. The bag shows a move as soon as it's made
(`client/predicted_bag.h`): the client does it to a copy of the bag with those rules and shows the
copy, until the server's bag comes back at a new revision, or for a second, if it doesn't, as when
the server refused. A health potion heals forty, one every ten seconds.

Monsters drop items, straight into their killer's bag: each kind's `loot` in the monsters file has
the items it can drop, the chance of each, and how many. A skeleton drops bones more often than
not, and now and then a potion, a leather cap, a tunic or boots, and the skeletons of the Ashlands and the
Crypt the Ash Blade and the buckler, the Crypt's more often. Bones are what recipes make things
from (see [Crafting](#crafting)). What stacks drops as a stack, and what
doesn't one at a time, each with rolls of its own, so two Ash Blades are seldom the same, and a
serial that no other item has. A line in
the chat says what the monster dropped, and what didn't fit, if the bag was full, which stays
behind. The world keeps those lines as it steps, and the server sends them after the tick.

Monsters drop gold too, each time, from their kind's `gold` in the monsters file: one to five for a
skeleton, three to nine for an Ash Skeleton, and six to fourteen for a Crypt Skeleton. It takes no
slot, so a full bag takes it too, up to a billion (`Inventory::kMostGold`), and the bag shows it
under its slots. It's part of the bag's message, `Inventory.gold`, so a change to it is a change to
the bag, which goes to the client and is saved with it (see [Saving players](saving.md)).

The server decides it all, as it does a swing. The client sends what you do with an item in
`Input`'s `item`, once, and the world moves the items, with `server/inventory.h`, or says why not,
in a line in the chat from nobody. A player can do ten things with items at once, then ten a
second. Each change takes the bag to its next revision, so telling whether a bag has changed takes a
number, not the bag: the server sends a player their bag, alone, over TCP, as `World`'s `inventory`,
as they join and when its revision moves, which is now and then. Nobody else gets it: others see
what's in your main hand once a second, as your weapon. An item that's gone from the items file
stays in the bag, where it can be moved but not worn or used, so taking one out of the file by
mistake loses nobody's.

## Quests

Some NPCs give quests, which ask you to kill a number of one kind of monster. In the Meadows, the
guard asks for three skeletons from around the gate, for 150 experience and two health potions. In
the Ashlands, the ash warden asks for five Ash Skeletons, for 400 experience and the Ash Buckler.
The healers, in both lands, ask for four from the Crypt. Each quest is in `content/quests.txtpb`: its name, its giver's
kind of NPC, what they say giving it, what it asks you to kill and how many, and the experience and
prizes it gives (see [Content](content.md)). A new quest of the same sort is another entry, and needs
no code.

An NPC with a quest for you has a gold mark over its head, whether its name shows or not, so that
it can be found: `!` for one you can take, or `?` for one you've done, to turn in. Click it to open
its quests: what it says, what each asks for and gives, and a button to take it, how many you've
killed for it, a button to turn it in once you've killed them all, or Done. The quests you're on
are on the right, under the minimap, or in its place with the minimap off, each with how many
you've killed for it, with a tick once that's all of them.

T, or the scroll among the buttons in the bottom right, opens your quests (`client/quest_window.h`):
those you're on, down the left, with a tick by each you've killed enough for, and under them those
you've done, the highest id first. The one picked shows on the right: who gave it, what they said,
what it asks you to kill, how far you've got, as a bar, and the experience and prizes it gives.
Give up asks whether you're sure, as what you've killed for it goes too, and Keep it says no.

- The server decides it all, as it does a skill a trainer teaches. The client sends `Input`'s
  `take` or `turn_in`, once, with the NPC's id and the quest's, and the world checks that the NPC
  is of the quest's giver's kind, in the same land, within talking reach, and that you can take or
  turn it in, or says why not, in a line in the chat from nobody.
- Giving up sends `Input`'s `give_up`, the quest's id, once, and needs no NPC near you. The world
  takes the quest off you, with your kills for it (`World::GiveUp`), and its giver has a `!` for
  you again: taking it starts it over, from none. A quest you're not on is a line from nobody,
  "You're not on that quest.", and one you've turned in stays done.
- A kill counts for each quest you're on that asks for its kind, up to as many as it asks for, with
  a line in the chat, such as "Bones by the Gate: 2 of 3 Skeletons killed.", and the last one says
  who to go back to. Only the killing blow counts, as with experience and loot.
- Turning a quest in gives its experience, which heals you at a new level, as a kill's does, and its
  prizes into your bag, rolled as loot is. They go into a copy of the bag first, so a bag without
  room for all of them gets none, and the quest waits until there's room. A quest turned in is
  done, and can't be taken again.
- Your quests go to you like your bag: alone, over TCP, as `World`'s `quests`, the ones you're on
  with your kills for each, and the ones you've done, as you join and when they change, which takes
  them to their next revision. Nobody else gets them.
- They're saved with the rest of you (see [Saving players](saving.md)).

## Chat

Enter opens a field at the bottom of the chat window, in the bottom left corner, and Enter again says
what you typed to everyone in your world, in all its lands, under your name. The chat's close button
folds it into a button in the corner, which counts the lines that have come since, and a click on it,
or Enter, opens it again. Escape leaves the field.
While you type, the game's keys do nothing, so a line doesn't walk you, turn the camera or swing
your sword.

- A line goes to the server with your input, as `say`. The server sends the lines said in each
  tick to each player when the tick ends, in one message, which has nothing but chat in it.
- Chat goes over TCP, even to a client that gets the world over UDP, as each line has to arrive,
  while a world that's lost is replaced by the next one.
- The server cleans each line (`protocol/chat.h`). Control characters, such as a new line, become
  spaces, so nobody can start a line that looks like someone else's, and the spaces at either end
  go. An empty line, or one longer than 200 bytes, goes nowhere. Protobuf refuses a line that
  isn't UTF-8.
- Each player can say five lines at once, and then one every two seconds, from a token bucket that
  fills up again over time (`server/token_bucket.h`). A line over that goes nowhere, and the server
  tells the player that they're talking too fast, in a line with no name, which the chat window
  shows in yellow.
- What the servers say, in a line with no name, such as that, a refusal or a guild's news, comes as
  a sentence the client translates, in the player's language, with the names in it as players gave
  them (see [Translations](translations.md#the-servers-sentences)). What players say stays as they
  said it.
- The client keeps the last hundred lines. The server prints each line said, and counts the lines,
  said and refused, in `nanommo_chat_lines_total`, which the dashboard shows a minute.

Each line goes to everyone in the world, so with a thousand players in a world, the server sends
each line a thousand times. A guild's lines go to its players alone (below).

## Guilds

G, or the shield on the button bar, opens the guild window. A guild is in one world, like its
characters, and a character is in one guild at most.

| Who | Can |
|---|---|
| Anyone in no guild | Found one, with a name of 3 to 24 letters, one space between words, which nobody else in the world has, whatever its case |
| The leader | Make a member an officer, or an officer a member, remove anyone, disband the guild, after a question, and take it to war with another, with `/war` (see [Guild wars](#guild-wars)) |
| Officers | Ask someone in, by the name of a character playing in the world, whatever its case, and remove members |
| Anyone in a guild | Talk to it, with `/g` and a space before the line, see its roster, and leave it |

- The guild window lists the guild's players, the highest rank first and then by name, with their
  level, and those playing now in white. A right-click on someone opens a menu of what your rank
  lets you do to theirs. The roster comes again as anyone in the guild joins or leaves it, or the
  world.

- An invitation is a question on the player's screen, with Join and Not now. It waits a minute,
  and a player has one at a time: another guild asking before they've answered is told they've
  been asked. Not now says no, so that another can ask.
- When the leader leaves, the highest rank left leads, and of those, who joined first. The last one
  out ends the guild, and its name is free again. Deleting a character leaves its guild, in the same transaction.
- While the guild is at war, nobody leaves it, nobody's removed from it, and it isn't disbanded,
  until the war's over (see [Guild wars](#guild-wars)).
- A player's guild shows under their name, in angle brackets, while their name shows (see
  [Names over heads](#names-over-heads)), unless the settings leave guilds out, and in their frame
  across from their name. It comes with their name, once a second.
- A guild's lines are green in the chat, after `[Guild]`. They go to its players in the world at
  once, over TCP, on the chat's own limit and cleaning.
- The guild's players hear who joined and who left. Anything refused is said in the chat, in yellow.
- Each guild action is a transaction in MySQL, which the world's tick waits for, so a player gets
  three at once, and then one a second.

## Trading

Two players in the same land trade items and gold. `/trade` and a name in the chat asks that
player, whatever its case, within talking reach, four metres (`World::kTradeReach`), and they
hear it as a line in the chat, with the `/trade` that says yes. Once each has asked the other, the
trade window opens on both screens, and the bag with it.

- What you offer is on the left: drag items onto it from the bag, a whole stack at a time, click
  one to take it back, and type the gold. What they offer is on the right.
- Ready says you'll trade what's offered, both ways, and each side's slots light green once it is.
  A change to either offer makes neither side ready, as there's something new to look at.
- Once both are ready, within four metres, they trade there and then: each gets what the other
  offered, all of it or none. If a bag hasn't room, neither changes, neither side is ready, and a
  line in the chat says why.
- Closing the window calls the trade off, and so does either player leaving the land or the game.
  A player trades with one other at a time.
- The bag keeps what you've offered where it is, faint, until the trade's over: it doesn't drag,
  use or wear it, and the server won't either.

The server decides it all, as it does a move in the bag. Both players are in the same copy of a
land, so each world keeps its trades (`server/trades.h`). The client sends what it does in
`Input`'s `trade`, once, and the world does it as the message comes, or says why not in a line
from nobody: an offer names the places in the bag, and the world keeps a copy of each stack as it
is then, which the other side sees. After each tick, each player whose trade opened, changed or
ended gets it, on its own, over TCP, as `World`'s `trade`, with nobody in it once it's over. A
trade moves things between bags, so it's on the bag's limit, and nothing trades while the world
hands its players over to its next instance, where a trade that was open is gone.

The server trusts none of what the client says about the trade, and checks it against the bags as
it has them:

- A place you offer from is held until the trade's over. The world refuses to move, use, wear or
  throw away what's there, or to put anything there: "You can't, while it's offered in your
  trade."
- Each change to either offer takes the trade to its next revision, which comes with the trade.
  A ready names the revision it was for, and one for an earlier revision does nothing, as the
  offers have changed since: "The trade has changed, so look at it again."
- Before it gives anything, the trade checks each place offered still has the same item, with the
  same rolls, and at least as many as were offered. A kill can add to a stack you've offered, and
  what it adds stays yours. If a place hasn't, nothing's given, and neither side is ready: "The
  items offered aren't all there any more."
- Once they've traded, both players are saved, together, with a record of the trade, in one Redis
  script, before either hears of it, and a player who leaves is saved as they go (see
  [Saving](saving.md)).

## Crafting

K opens the crafting window, wherever you are: there's no forge or workbench to go to, and nobody
to craft for you. A recipe makes something from what's in your bag, where you stand. You know every
recipe of your level or below, so there's none to learn. The window lists them, each with what it
takes, as how many you have in your bag of how many it needs, in green once you've enough, what it
makes, and in how long, and a Craft button, which is off while you can't.

| Recipe | Takes | Makes | From level | In |
|---|---|---|---|---|
| Carve a Bone Buckler | 12 bones | A Bone Buckler, for the off hand, with an armour of 1 | 1 | 5 s |
| Bind a Bone Sword | The sword and 10 bones | A Bone Sword, with 18 damage and 1.7 metres of reach | 2 | 8 s |
| Temper an Ash Blade | A Bone Sword and 20 bones | An Ash Blade, which rolls its damage as a dropped one does | 4 | 12 s |

The sword is the one a new player starts with, bones drop from every skeleton, fifty to a slot, and
the Ash Blade drops in the Ashlands and the Crypt too, now and then.

- A recipe takes from the bag, a stack at a time from the first slot, and never from what you wear,
  so the sword in your hand goes into the bag first. What you offer in a trade isn't there to take
  either, as the trade holds it.
- While you make something, a bar over the hotbar fills, with the item's name on it, and the
  window's Stop button stops it. So does moving, a walk you asked for, a swing or a skill, a blow,
  from a monster or another player, dying, a trade that opens, a duel you say yes to, and leaving
  the land or the game: "You stopped making a Bone Sword." Nothing's taken until it's made, so
  stopping loses nothing. You make one thing at a time.
- Once its time is up, the world checks your bag again, as it may have changed since: all it
  takes, and room for what it makes once that's gone. Then it takes the one and gives the other, as
  one change to the bag, which is saved once, and the chat says "You made a Bone Sword." What
  doesn't stack gets a serial and rolls of its own, as loot does, and what was taken goes, serial
  and all. If the bag isn't enough any more, nothing changes, and the chat says why.

The server decides it all. The client sends `Input`'s `craft`, once, to start making a recipe or to
stop, and the world starts it, or says why not, in a line from nobody: "You can make a Bone Sword
from level 2.", "You need 10 Bones in your bag to make a Bone Sword." or "Your bag is full. Make
room for a Bone Sword first." A player can start or stop three times at once, and then once a
second, and nothing starts while the world hands its players over to its next instance. What you're
making, and how long you've been at it, comes in your own part of each world, as your wait to come
back does (see [Networking](networking.md#what-crafting-sends)). Each thing made is an event for the
analytics, `craft`, with the item and how many. The recipes are in `content/recipes.txtpb` (see
[Content](content.md)), and a new one is another entry, which needs no code.

## Duels

Two players in the same land fight each other in a duel, once both have said yes to it, and
nobody hurts another player otherwise, but in a [guild war](#guild-wars). `/duel` and a name in the chat asks that player, whatever
its case, within ten metres (`Duels::kReach`). They hear it as a line in the chat, and a question on
their screen: Duel says yes, and Not now, or closing it, says no, as `/duel yes` and `/duel no` do,
and asking back the one who asked is a yes too. An ask waits thirty seconds, and then it's gone,
and the one who asked hears that nobody answered.

- A yes puts the flag down where the one who asked stands: a gold pole, with a ring at its foot and
  a fainter one 25 metres out (`Duels::kFlagReach`). A countdown of three seconds shows in the
  middle of both screens, then Fight!
- Each can pick the other then, as they would a monster: a click on them, which gives them a red
  ring at their feet and their frame beside yours, and hit them with a swing or a skill, which runs
  you up to them first, out of its reach, as it does to a monster. A skill that hits all around hits
  them too. Nobody else can be picked, so nobody else hits either of them, and neither hits anyone
  else, but an enemy at war. Monsters fight on as before.
- Your screen draws your opponent a moment in the past, as it does everyone else, so your blows on
  them are checked against where your screen had them, up to a fifth of a second back: a blow that
  looked as if it would reach someone running off lands, and one that looked short of someone
  coming closer misses ([Networking](networking.md#blows-where-the-attacker-saw-them)).
- The blow that would leave a duellist no health leaves them one, and they've lost: nobody dies in a
  duel. A duellist who goes more than 25 metres from the flag, or dies, to a monster, loses too, and
  so does one who types `/duel yield`. Both hear who won, in the chat: "alice won the duel." and
  "You won the duel against bob."
- Health comes back after a duel as it does after a fight with monsters, from a potion, a skill or a
  new level, and not otherwise. So the next duel starts with what the last one left, and a loser who
  duels again at one health loses to the first blow.
- A player is in one duel at a time, from the ask to the end. `/duel yield` before the fight calls
  it off, and so does either player leaving the land or the game, or the world handing its players
  over to its next instance.
- A blow doesn't push or throw a player, as their own keys move them, and a duel gives no
  experience, and drops nothing.

The server decides it all, as it does a trade. Both players are in the same copy of a land, so each
world keeps its duels (`server/duels.h`). The client sends what it does in `Input`'s `duel`, once,
and the world does it as the message comes, or says why not in a line from nobody: three at once,
then one a second. After each tick, each player whose duel was asked for, counted down a second,
started or ended gets it, on its own, over TCP, as `World`'s `duel`, with nobody in it once it's
over. The player you pick comes with each input, as `Input`'s `target_player`, beside the monster's
`target`, and the world keeps it only for your opponent in a duel that's started, or an enemy at
war, in place of a monster: picking anyone else picks nobody. Why a duel ends at one health, and the opponent is picked
beside the monster, is [decision 33](decisions/0033-players-fight-each-other-in-duels-that-end-at-one-health.md).

## Guild wars

Two guilds in the same world go to war once both their leaders say so, and for half an hour, the
players of the two can hurt each other anywhere, in any land, without asking. `/war` and a guild's
name in the chat, whatever its case, asks that guild, as your guild's leader, and its players in
the world hear it in their guild's chat: "Iron Wolves asks Stone Bears to war. Your leader types
/war Iron Wolves to say yes." An ask waits a minute, and a guild asks another once until then. Their
leader asking back is the yes, as in a duel, and both guilds hear "Iron Wolves and Stone Bears are
at war, for 30 minutes."

- A guild is in one war at a time. A war ends when its time is up (`Guilds::kWarLength`), and both
  guilds hear "The war between Iron Wolves and Stone Bears is over." Nobody can give one up.
- The enemy guild's players' names are red, over their heads while the pointer's on one or you've
  picked them, or always, with Other players on in the settings (see
  [Names over heads](#names-over-heads)), and a click on one picks them as it picks a monster, or a
  duel opponent: a red ring at their feet, their frame beside yours, and the hotbar's skills for
  one target ready to use on them. A skill that hits all around hits every enemy it reaches, with
  the monsters, and never your guildmates or anyone else. Nobody else can hurt a player, but a duel
  opponent.
- Your blows on an enemy are checked where your screen drew them, up to a fifth of a second back,
  as in a duel ([Networking](networking.md#blows-where-the-attacker-saw-them)), and a blow doesn't
  push or throw a player.
- A war's blows kill: the enemy with no health left dies, as to a monster, and comes back in the
  middle of their land, at full health, after ten seconds. A kill gives no experience and drops
  nothing, and it's the killing blow's player's alone. A duellist an enemy kills loses the duel.
- Each kill is a line in both guilds' chat, green, after `[Guild]`: "arthur killed mordred."
- A player scores for the war once every five minutes at most (`Scoring::kOnceEvery`), whoever
  kills them. Killed again sooner, they die, and both guilds hear of it, but the score doesn't
  move, so two friends on either side can't run up a score with one feeding the other kills.
- Nobody leaves a guild at war, so that a losing side can't dodge the war, nor disband to take the
  war and its kills with it. While the war lasts, a member can't leave, nobody can be removed, the
  leader can't disband the guild, and a character in it can't be deleted: "Nobody leaves a guild at
  war until the war's over." Once it's over, they can.
- The guild window shows the war under the guild's name: the enemy, in red, the kills both ways,
  from your side, and how long it has left. The score comes from MySQL, so it's a few seconds behind
  the game, as the roster's levels are.

The world's rules know nothing of MySQL, so the server tells each world each player's guild, as
they join it, leave it or it ends, and the world's wars, as they change, and `World::Hostile` says
who may hurt whom: a duel opponent, or a player of the guild at war with yours. A war is a row in
`guild_wars`, from V14, as guilds are rows of their own, and an ask is a key in Redis,
`war_ask:<world>:<guild>:<enemy>`, for a minute, as an invitation is. The yes is a transaction that
locks both guilds' rows, the lower id first, before the leader's own. The server reads the world's
wars again every five seconds, in the tick, so that a war accepted on the other instance during a
handover, or one whose time is up, takes effect: a war can go on for up to five seconds past its half
hour. Each kill that scores goes to the saver with the tick's save, as a row of its own for the
war's score ([Saving players](saving.md#guild-wars)). Why wars take two guilds, last half an hour and
kill is [decision 41](decisions/0041-guilds-at-war-hurt-each-other-anywhere.md), and why a kill
counts once, a victim scores once every five minutes, and nobody leaves a war is
[decision 42](decisions/0042-a-war-kill-counts-once-and-nobody-leaves-a-war.md).

## Names over heads

A name shows over someone's head while the pointer's on them, or you've picked them, and not
otherwise, so that a crowd of players and monsters doesn't hide the land under its names. The
pointer's on whoever's nearest along it, but you, of any kind (`PickAnyone`, in
`client/picking.h`), and what you've picked is the monster, the duel opponent or the enemy at war
you clicked. An NPC with a quest for you shows its mark anyway, without its name, so that it can be
found.

The settings' Interface page, on O, has the rest, under "Names over heads":

| Setting | What it does | To begin with |
|---|---|---|
| Always show them over: Monsters, Other players, NPCs, You | Shows those names all the time | Each off |
| With them: Health | A health bar under each name, but an NPC's | On |
| With them: Levels | The level before each name, but an NPC's | On |
| With them: Guilds | A player's guild under their name, in angle brackets | On |

- A name shows within 50 metres, in front of the camera, as before, in red for an enemy at war.
- A monster's skill on its way shows under its health while its name does, and its shape on the
  ground either way (see [Monsters' skills](#monsters-skills)).
- The client keeps them in `settings.txtpb` (see [Music](graphics.md#music)), as `Interface`'s
  `nameplates`, 4, a `Nameplates` with a switch for each, in `client/settings.proto`. A file from
  before has none, so it gets the defaults.
- Until 0.94.0, every name within 50 metres showed, with its level and health, but an NPC's. Why
  names show for whoever you notice, and why the hints went into the help, is
  [decision 44](decisions/0044-looks-are-chosen-once-and-names-show-over-whoever-is-pointed-at-or-picked.md).

## Help and the settings

H, or the question mark among the buttons in the bottom right, opens the help
(`client/help_window.h`): a page each for moving, fighting, items, quests, guilds and duels, chat,
and the keys, each a few lines on what to do. The keys' page is the settings' own list (`BuildKeys`,
in `client/menu.h`), so the two say the same. The help has the hints that were on the screen: the
bag's line on throwing things away, and the map's on clicking a place to walk there. The bag no
longer counts the slots used.

O opens the settings, as Settings in the menu does, and closes them again. Their Interface page has
the language, English or Romanian (see [Translations](translations.md)), the interface's size, "Show
the minimap", which can take the minimap off the screen, and whether it
turns with the camera (see [The map and the minimap](graphics.md#the-map-and-the-minimap)), the
hotbar's order, and the names over heads (above). The client keeps whether the minimap shows as
`Interface`'s `minimap`, 5.

The buttons in the bottom right, round like the chat's, open the character (C), the bag (I), your
quests (T), the guild (G), the map (M), the help (H) and the menu (Escape), each with its key in its
tooltip. Crafting is on K.
