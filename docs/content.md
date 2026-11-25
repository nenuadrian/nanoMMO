# Content

The game's content is data, in the `content` folder, which the world servers, the client and the
bots read, so that there's one place to change it. The folder holds `factions.txtpb`, `figures.txtpb`,
`items.txtpb`, `monsters.txtpb`, `npcs.txtpb`, `quests.txtpb`, `recipes.txtpb`, `rig.txtpb` and
`skills.txtpb`, in protobuf's text format, each with its schema next to it, in `factions.proto`,
`figures.proto`, `items.proto`, `monsters.proto`, `npcs.proto`, `quests.proto`, `recipes.proto`,
`rig.proto` and `skills.proto`. The figures are the players' bodies and
hair, and the rig is the skeleton they're built on, and how it stands, idles, walks, runs and dies
(see [Characters](graphics.md#characters)). The thirteen items are the sword, a health potion, a
bone, a leather cap and tunic, the Ashlands' blade and buckler, the clothes a new player starts in,
a linen shirt, cloth trousers and plain shoes, leather boots, and a bone buckler and a bone sword,
which recipes make. The four kinds of monster are the skeleton, the Ash Skeleton and the Crypt
Skeleton, the last two with a skill each, and a boss, the Bone Colossus, with four (see
[The world boss](gameplay.md#the-world-boss)). There are nine kinds of NPC, three quests (see
[Quests](gameplay.md#quests)), sixteen skills (see [Skills](gameplay.md#skills)), three recipes (see
[Crafting](gameplay.md#crafting)) and three factions (see [Factions](gameplay.md#factions)).

- An item, or a kind of monster or NPC, has an id and a name, and parts that each side reads what it
  needs from. A weapon's `weapon` and a monster's `fight` are the server's, with reach, swing and
  hit times, damage, and for a monster, its sight, its leash and how far it wanders. A `look` is the
  client's, with an item's or a monster's model, its length or height, where a weapon is held, an
  item's icon and tint, a monster's tint, and an NPC's skin, hair colour, way to wear its hair,
  weapon and clothes, and the colour of what it wears that's dyed. A monster's level, health,
  experience, loot and gold are the server's too.
- A kind of monster's `skills` are big blows it telegraphs (see
  [Monsters' skills](gameplay.md#monsters-skills)), each with an `id` from one among its kind's, a
  `name`, and a `shape`, which is `CIRCLE`, all round the monster, or `CONE`, in front of it, as
  wide as its `arc`, in degrees. Its `size` is how far out the shape goes, in metres, `telegraph_ms`
  how long it shows before the skill lands, and `color` its colour, which the clients draw, while
  its `damage`, its `cooldown_ms`, from when it starts, and its `range`, how near the player the
  monster fights has to be for it to start, are the server's.
- A kind with `phases` is a boss. Each phase has the `health` it starts at, in percent of the
  boss's most, the first at 100, its `skills`, by their ids among its kind's, which the boss uses
  in that phase alone, in the kind's order, and its `adds`, so many monsters of a `kind`, by its
  id, which come round the boss as the phase starts. A kind's `respawn_ms` is how long it lies dead,
  ten seconds without one. Where each phase starts is the clients' too, as a boss's bar marks it,
  while its skills, its adds and the respawn are the server's.
- An item is the parts it has, and no type. `stack` says how many go in a slot of a bag, `equip`
  where it's worn and what it adds, as `modifiers` of `STAT_DAMAGE`, `STAT_HEALTH` or
  `STAT_ARMOUR`, `use` what using one does and how often, and `rolls` the modifiers each one gets
  as it's made, from `min` to `max`. A new kind of item that puts parts we have together is
  another entry, and needs no code. The kit, `starting`, is what a new player starts with, and
  `retired` lists the ids of items that are gone, which no item can take again. A skill's `effect` is
  the server's, and its `look` the client's, with its icon, its move and its fx. The client shows a
  skill's level and cooldown too, so those are in both. `starting` names the skills every new
  player knows, `attack` the swing on Space, which every player knows, and a kind of NPC's
  `teaches` the skills it teaches, by id.
- A recipe is something a player can make from their bag, wherever they are. It has its `inputs`,
  each an `item` and a `count`, which it takes, its `output`, the same, which it makes, how many
  `seconds` that takes, and the `level` it needs. Every player knows each recipe of their level or
  below, so there's no list of who knows what. Each of its items is at most a slot's worth, so that
  what it makes always has room in a slot that what it takes has emptied.
- A faction is a side a player swears to, for good. It has its `id`, its `name`, its `color`, as
  0xRRGGBBAA, which marks its players by their names, and its `herald`, the kind of NPC who swears
  players to it, by id. A herald is a kind of NPC like any other, dressed in its faction's colour
  in the NPCs file, so a faction with no herald standing anywhere can't be joined. A player's
  faction is saved by its id.
- The figures file's skins, hair colours and ways to wear the hair, its `hairs`, and the rig's
  `builds` are what a player chooses from as they make their character (see
  [Making a character](gameplay.md#making-a-character)). A `Hair`'s `name` and a `Build`'s are
  what that screen calls them, from "Short" to "Bald", and "Slight" and "Broad", or, without one,
  its number. A character's looks are places in those lists, so a new one goes at the end, as one
  put in before the end, or taken out, changes the looks of everyone who chose one after it. A
  character from before there were looks to choose has the looks its id picks, by how many each
  list has, so any new one changes most of theirs.
- The names, and the quests' words, are English, which `ClientHash` hashes and the servers say.
  The client translates each as it shows it, by its kind and id, such as `item 8`, so a language
  changes nothing in the content (see [Translations](translations.md#how-it-works)), and a
  server's sentence names the content by its id, for the client to say in its player's language.
  Each item has a plural too, "a Bone" and "$0 Bones", for so many of it in a server's sentence.
- Monsters with a fixed home, camps of monsters and NPCs name their kind by id, and their land by
  name. A quest names its giver's kind of NPC, the kind of monster it asks for and its prizes by id,
  a recipe what it takes and makes, and a faction its herald's kind of NPC.
- Each file's first two lines, `# proto-file:` and `# proto-message:`, name its schema, so that
  editors and tools that know text protos can check it and complete its fields.
- `content/content.h` (`nanommo_content`) reads the folder with protobuf's `TextFormat`, so a field
  the schema doesn't have, or a value of the wrong type, is an error with the file and the line it's
  on. Then it checks what types can't, namely that every item and kind has an id, no two share one,
  an item's parts go together (a weapon is held in the main hand, what's worn has a slot, and what
  stacks isn't worn and doesn't roll), a roll goes from less to more, the kit's items are in the
  file and fit in a slot, no item has a retired id, each spawn and camps names a kind and a land we
  have, no monster with a fixed home has a leash that reaches the middle of its land, where players
  come back to life, each skill has a level and lands before it's over, each starting skill is in
  the file, each monster's skill has an id of its own among its kind's, a size, an arc if it's a
  cone, a range of more than nothing and no more than its size, so that whoever it starts on is in
  it, a telegraph of half a second at least (`kShortestTelegraph`), and a cooldown no shorter than
  that, each boss's first phase starts at 100%, each phase after it below the one before and above
  nothing, uses skills its kind has, and calls up some of a kind in the file, and each
  drop has a chance from nothing to one, and goes from fewer to more, as a monster's gold goes from
  less to more, each kind of NPC has a skin, each quest has a giver, asks for some kills, and
  gives some of each of its prizes, and each recipe has a level, takes some time and something,
  and takes and makes some of each of its items, and each faction has a colour and a herald, which
  no other has. Once every
  file is read, `CheckContent` checks what no file can on its own, that each item a monster
  drops is in the items file, each skill a trainer teaches is in the skills file, each NPC's hair is
  in the figures file and its clothes in the items file, each quest's giver, monster and prizes are in
  theirs, each item a recipe takes or makes is in the items file, no more of it than go in a slot,
  each part of a figure, a way to wear the hair or an item is on a joint of the rig, and each
  faction's herald is in the NPCs file. The server, the client and the bots don't start without good content.
- Ids start at one, and an item or a kind keeps its id for good, as they're saved and sent by id.
  Protobuf leaves a zero out, so the client takes a missing weapon or kind as one it already knows.
- Each player's bag holds its items by their ids (see [Items](gameplay.md#items)), saved with the rest of the
  player (see [Saving players](saving.md)). A player who's never had a bag gets the kit, so
  the database doesn't need to know what's in it. What's in a player's main hand is still saved
  in the `weapon` column of the `players` table from V5, for the servers from before bags.
- Players' weapons, and monsters and NPCs, go to the clients by id, once a second, with the names
  and levels, and the client looks up the name and the look in its content. The fields that carried
  them before, a monster's name and an NPC's name, colour, radius and whether it talks, are reserved
  in `nanommo.proto`, so that no new field takes their numbers. The client draws a monster of a kind
  it doesn't have as a cube.
- The world servers and the bots read the folder from `content`, under where they start, or from
  `NANOMMO_CONTENT`, which their images set to their own copy, in `/usr/local/share/nanommo/content`.
  The build copies the files next to the client, which reads them from there.

Each side reads its own copy, so a client and a world have to agree on what the client reads. Each
world works out its `ClientHash`, a 64-bit FNV-1a of the figures and the rig, which the clients
draw the players from, so that a character's looks draw the same on each, and of the ids, names
and looks, what the bag shows of each item, which is all of it but its weapon, the skills' levels
and cooldowns, the monsters'
skills but for their damage, cooldowns and ranges, where each boss's phases start, what each trainer teaches, and all of each quest, each recipe and each faction, serialized deterministically, registers it with Consul, and the login server
lists it with the world. The fights and the skills' effects stay out of it, so that changing a
sword's damage or a skeleton's leash needs no new client.
A client lists a world with other content than its own as such, and doesn't log in to it. The servers
and the client print their hash as they start.

A change to the content, such as the sword's reach, is an edit to a file and a restart of the world
servers, and a rebuild of the client when it changes what the client reads. A new item or kind is
another entry, with the next id, as the Ash Skeleton was, and a monster drops it once its kind's
loot names it.

## Checked as a graph

What `CheckContent` can't tell is whether a player can ever get to each thing, and so whether
there's a quest whose giver stands in no land, a monster with no spawn or camps, an item nothing
drops, gives or makes, or a recipe that takes one. Nor whether recipes go round, making something
from nothing. The content tests (`content/graph_test.cc`) check that, as a graph, in
[smollerGraph](https://github.com/nenuadrian/smollerGraph), the graph database from its own series,
which the build fetches at a version
([decision 40](decisions/0040-check-the-content-as-a-graph-with-smollergraph.md)).

- `AddContentGraph` (`content/graph.h`) adds a node for a new player and for each land, kind of
  monster and NPC, quest, item, skill, recipe and faction, labelled `Player`, `Land`, `Monster`,
  `Npc`, `Quest`, `Item`, `Skill`, `Recipe` and `Faction`, and a relationship for each way a player
  gets from one to the next. They're `STARTS_IN`, `STARTS_WITH`, `KNOWS`, `GATE`, `SPAWNS`, `STANDS`,
  `DROPS`, `SUMMONS`, `GIVES`, `REWARDS`, `TEACHES`, `NEXT`, `CRAFTS`, `MAKES` and `ENLISTS`, from a
  herald to their faction. A quest `ASKS_FOR` its monster, which is what finishing it takes.
- Each item a recipe takes goes `INTO` it, which isn't a way, as a path reaches a node through any
  one way in, and a recipe needs all it takes. So `AddContentGraph` ends in rounds. Each asks for
  the items a new player reaches, by the shortest paths `Unreachable` asks for, and adds a `CRAFTS`
  from the new player to each recipe whose items they all are, so that what it `MAKES` is reached.
  What that makes may go into another recipe, as the Bone Sword goes into the Ash Blade, so the
  rounds go on until one crafts nothing new. The game's recipes take two, and a third that crafts
  nothing.
- `Unreachable` lists each node no path from a new player reaches, and each quest whose monster is
  out of reach, so a faction whose herald stands in no land is "Victoria, a faction". The test on
  the game's content expects none.
- `Cycles` lists each item that goes back into itself through recipes, with the way round, such as
  `Bone goes back into itself: (Bone)-[:INTO]->(Bind a Bone Sword)-[:MAKES]->(Bone Sword)-[:INTO]->...`.
  smollerGraph's Cypher can't give one variable to both ends of a path, and its shortest path from a
  node to itself is the node alone, so it's two queries, joined in C++. One is a shortest path from
  each recipe to each item it leads to, along `MAKES` and `INTO`, and the other each item `INTO`
  each recipe. An item whose recipe leads back to it goes round. The test on the game's content
  expects none.
- `nanommo_graph content content.sg` writes the graph, and prints what's out of reach and what goes
  round, with an exit code of 1 when there's anything. smollerGraph's `sg`, built from our build with
  `cmake --build build --target sg`, serves it with its explorer,
  run as `build/bin/sg serve content.sg 8474`.

A new kind of link in the content needs its relationship in `content/graph.cc` too.
