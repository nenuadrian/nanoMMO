# Content

The game's content is data, in the `content` folder, which the world servers, the client and the
bots read, so that there's one place to change it: `figures.txtpb`, `items.txtpb`,
`monsters.txtpb`, `npcs.txtpb`, `quests.txtpb` and `skills.txtpb`, in protobuf's text format, each
with its schema next to it, in `figures.proto`, `items.proto`, `monsters.proto`, `npcs.proto`,
`quests.proto` and `skills.proto`. The figures are the players' bodies and hair (see
[Characters](graphics.md#characters)). There are eleven items: the sword, a health potion, a bone, a leather
cap and tunic, the Ashlands' blade and buckler, the clothes a new player starts in, a linen shirt,
cloth trousers and plain shoes, and leather boots, three kinds of monster, the skeleton, the Ash
Skeleton and the Crypt Skeleton, six kinds of NPC, three quests (see [Quests](gameplay.md#quests)) and four
skills (see [Skills](gameplay.md#skills)).

- An item, or a kind of monster or NPC, has an id and a name, and parts that each side reads what it
  needs from. A weapon's `weapon` and a monster's `fight` are the server's: reach, swing and hit
  times, damage, and for a monster, its sight, its leash and how far it wanders. A `look` is the
  client's: an item's or a monster's model, its length or height, where a weapon is held, an
  item's icon and tint, a monster's tint, and an NPC's skin, hair colour, way to wear its hair,
  weapon and clothes, and the colour of what it wears that's dyed. A monster's level, health,
  experience and loot are the server's too.
- An item is the parts it has, and no type: `stack` says how many go in a slot of a bag, `equip`
  where it's worn and what it adds, as `modifiers` of `STAT_DAMAGE`, `STAT_HEALTH` or
  `STAT_ARMOUR`, `use` what using one does and how often, and `rolls` the modifiers each one gets
  as it's made, from `min` to `max`. A new kind of item that puts parts we have together is
  another entry, and needs no code. The kit, `starting`, is what a new player starts with, and
  `retired` lists the ids of items that are gone, which no item can take again. A skill's `effect` is
  the server's, and its `look` the client's: its icon, its move and its fx. The client shows a
  skill's level and cooldown too, so those are in both. `starting` names the skills every new
  player knows, `attack` the swing on Space, which every player knows, and a kind of NPC's
  `teaches` the skills it teaches, by id.
- Monsters with a fixed home, camps of monsters and NPCs name their kind by id, and their land by
  name. A quest names its giver's kind of NPC, the kind of monster it asks for and its prizes by id.
- Each file's first two lines, `# proto-file:` and `# proto-message:`, name its schema, so that
  editors and tools that know text protos can check it and complete its fields.
- `content/content.h` (`nanommo_content`) reads the folder with protobuf's `TextFormat`, so a field
  the schema doesn't have, or a value of the wrong type, is an error with the file and the line it's
  on. Then it checks what types can't: every item and kind has an id, no two share one, an item's
  parts go together (a weapon is held in the main hand, what's worn has a slot, and what stacks
  isn't worn and doesn't roll), a roll goes from less to more, the kit's items are in the file and
  fit in a slot, no item has a retired id, each spawn and camps names a kind and a land we have, no monster with a
  fixed home has a leash that reaches the middle of its land, where players come back to life,
  each skill has a level and lands before it's over, each starting skill is in the file, and each
  drop has a chance from nothing to one, and goes from fewer to more, each kind of NPC has a skin,
  and each quest has a giver, asks for some kills, and gives some of each of its prizes. Once every
  file is read, `CheckContent` checks what no file can on its own: that each item a monster drops is
  in the items file, each skill a trainer teaches is in the skills file, each NPC's hair is in the
  figures file and its clothes in the items file, and each quest's giver, monster and prizes are in
  theirs. The server, the client and the bots don't start without good content.
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
world works out its `ClientHash`, a 64-bit FNV-1a of the ids, names and looks, what the bag shows of
each item, which is all of it but its weapon, the skills' levels and cooldowns, what each trainer
teaches, and all of each quest, serialized deterministically, registers it with Consul, and the login server
lists it with the world. The fights and the skills' effects stay out of it, so that changing a
sword's damage or a skeleton's leash needs no new client.
A client lists a world with other content than its own as such, and doesn't log in to it. The servers
and the client print their hash as they start.

A change to the content, such as the sword's reach, is an edit to a file and a restart of the world
servers, and a rebuild of the client when it changes what the client reads. A new item or kind is
another entry, with the next id, as the Ash Skeleton was, and a monster drops it once its kind's
loot names it.
