# Gameplay

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

Click a monster to pick it: it gets a red ring at its feet, and a frame beside yours with its
name, level and health. A click on the ground lets it go, and so does Escape, its death, or going
into another land. The cursor is a crosshair over a living monster, a hand over an NPC within
talking reach, and the arrow elsewhere. Each input carries the monster you picked (`Input.target`),
which the world keeps as your target.

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
its leash of the monster's home, four metres. A monster that's hurt goes for whoever hurt it last
instead, seen or not, while they're within its leash, so that a kick that knocks it out of sight
doesn't make it forget them. The content's checks keep each fixed home further than that from the middle of the
island, so the middle, where players start and come back, is safe. A monster runs at them at the
players' running speed, and swings once it's within 1.2 metres. The skeleton's swing takes 0.9 seconds, its attack animation, and hits for five,
0.65 seconds in, less the armour you wear, down to one. An Ash Skeleton, in the Ashlands, is level two, and hits for eight. Run, or die, and it gives up and wanders back.

A monster with no health left dies, and whoever killed it gets its experience, fifty for a skeleton, and ninety for an Ash Skeleton,
and what it drops, into their bag (see [Items](#items)).
Going up a level fills your health, to the new level's most. A player with no health left dies. The
dead play their death animation and lie there for ten seconds (`World::kRespawnTime`). Then a
monster comes back where it spawned, at full health. A player can come back once the wait is over,
and stays down until they ask to: the world refuses before then. While you're dead, the world
darkens, and a window in the middle counts down the seconds the server says you have left to a
Respawn button, which brings you back in the middle of your land, at full health. The screen goes
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
Each shows its icon and its key, and its name and level when the mouse is over it. A skill you don't
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
off, with Shift for half the stack. Double-click it to drink it, wear it or take it off, and
right-click it to throw it away. A health potion heals forty, one every ten seconds.

Monsters drop items, straight into their killer's bag: each kind's `loot` in the monsters file has
the items it can drop, the chance of each, and how many. A skeleton drops bones more often than
not, and now and then a potion, a leather cap, a tunic or boots, and the skeletons of the Ashlands and the
Crypt the Ash Blade and the buckler, the Crypt's more often. What stacks drops as a stack, and what
doesn't one at a time, each with rolls of its own, so two Ash Blades are seldom the same. A line in
the chat says what the monster dropped, and what didn't fit, if the bag was full, which stays
behind. The world keeps those lines as it steps, and the server sends them after the tick.

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

An NPC with a quest for you has a gold mark over its name: `!` for one you can take, or `?` for one
you've done, to turn in. Click it to open its quests: what it says, what each asks for and gives,
and a button to take it, how many you've killed for it, a button to turn it in once you've killed
them all, or Done. The quests you're on are on the right, under the minimap, each with how many
you've killed for it, with a tick once that's all of them.

- The server decides it all, as it does a skill a trainer teaches. The client sends `Input`'s
  `take` or `turn_in`, once, with the NPC's id and the quest's, and the world checks that the NPC
  is of the quest's giver's kind, in the same land, within talking reach, and that you can take or
  turn it in, or says why not, in a line in the chat from nobody.
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
what you typed to everyone in your world, in all its lands, under your name. Escape leaves the field.
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
- The client keeps the last hundred lines. The server prints each line said, and counts the lines,
  said and refused, in `nanommo_chat_lines_total`, which the dashboard shows a minute.

Each line goes to everyone in the world, so with a thousand players in a world, the server sends
each line a thousand times. Chat for the players nearby, or for a party or a guild, would send each
line to far fewer.
