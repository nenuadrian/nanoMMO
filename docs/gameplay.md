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
around the middle of each land, and up to three more at each of eight camps, which the land's seed
places between the beach and the rock, at least thirty metres from the middle. It runs their AI each
tick: a monster rests for two to six seconds, walks to a random place within its kind's wander of where
it spawned, at 1.8 metres a second, and rests again. If the sea or a tree stops it, it rests, and
picks another place. The world's random generator takes a seed, so the tests pick theirs.

## Fighting

Space swings your weapon at the nearest monster within its reach, and turns you to face it. The
sword's numbers are in the items file: it reaches 1.6 metres, and a swing takes two seconds, the
length of the slash animation, and you stand still until it's over. The sword hits 0.9 seconds in, as
it comes across the front, if the monster is still alive and in reach, for fifteen health, and each
level after the first adds five to whatever you hold, and what you wear adds its damage. With no
weapon in your main hand, Space does nothing, and putting the weapon away ends a swing. Holding Space down swings again and again. All
of it happens on the server: the client only says that Space is down, in `Input`.

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

A skill is a move with a cooldown. Each player knows the thrust from the start, and learns the
others from a trainer, from their levels on (see [Learning skills](#learning-skills)). The four in
`content/skills.txtpb` are a skill a level, from one to four:

| Skill | From level | Cooldown | What it does |
|---|---|---|---|
| Thrust | 1 | 8 s | 35 damage to the nearest monster within 2.4 metres |
| Kick | 2 | 6 s | 10 damage, and knocks the monster 3 metres back |
| Second Wind | 3 | 30 s | Heals 40, up to your most |
| Whirlwind | 4 | 12 s | 25 damage to each monster within 2.5 metres |

Each level after the first adds five to a skill's damage, as it does to a weapon's. There's no
skill tree: a trainer teaches each of its skills to any player who has reached its level.

The hotbar along the bottom of the screen has the skills in the order of their levels, each on a
number key, 1 for the first, up to 9, and a click on one uses it too. Each shows its icon and its key,
and its name and level when the mouse is over it. A skill you don't know yet is greyed out, with
the level it needs, or Learn once you've reached it, and one that's cooling down is shaded, a slice that shrinks clockwise from
the top, with the seconds left in the middle. The client works that out from the milliseconds the
server sends and the cooldown in the skills file.

The server decides it all, as it does a swing. The client sends a skill's id in `Input` once, as its
key goes down, and the world keeps it until the player is free: a skill asked for during a swing or
another skill starts as that ends. Then it starts if the player knows it and its cooldown is over,
and otherwise it goes. A skill holds the player for its cast time, like a swing,
lands partway through, on the monster the player turned to as it started if it's still in reach,
or on each one in reach, and starts its cooldown as it starts. Cooldowns run down whatever the player does, dead or alive,
and aren't saved: a player who leaves and comes back finds their skills ready.

A player using a skill has the motion `MOTION_SKILL`, and its id, so that everyone's clients can
show it. The skills file names each skill's clip, which the client finds in the characters by name,
and plays once, from the start: Thrust plays Mixamo's Thrust Slash, Kick its Kicking, and Whirlwind
its Standing Melee Attack 360 Low, each about as long as the skill holds the player. Second Wind has no
clip, so the player stands, in a green glow, see-through and drawn after the sea, that grows over
the first quarter of a second. One skill straight after another blends from one clip into the next,
as a change of motion does. Your cooldowns are yours alone, so they go with your experience, after the world: the
milliseconds left for each skill that isn't ready, by its id.

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

Press I, or choose View, Bag, to open the bag. What you wear is in the character window (C). Each
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
them all, or Done. The quests you're on are in the top right corner, each with how many you've
killed for it, with a tick once that's all of them.

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
