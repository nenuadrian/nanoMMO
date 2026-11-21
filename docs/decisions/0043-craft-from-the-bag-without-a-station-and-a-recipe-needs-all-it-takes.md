# 43. Craft from the bag, without a station, and a recipe needs all it takes

## Status

Accepted

## Context

Bones drop from every skeleton, more often than not, fifty to a slot of the bag, and until episode
93 nothing used them. Episode 93 gives players crafting: recipes that make an item from others, as
most MMOs have, where they're usually made at a forge or a workbench, from recipes a player finds
or buys and learns, by a crafting skill that levels up, with a chance to fail.

Crafting changes the bag, which a trade had already shown can be made to give an item twice
([0028](0028-a-trade-is-checked-against-the-bags-and-saved-as-one.md)): what's taken and what's
made have to change together, against the bag as it is, with what's offered in a trade held where
it is.

Recipes are content, and so links in the content graph
([0040](0040-check-the-content-as-a-graph-with-smollergraph.md)), which a new link has to go into,
or what it leads to looks out of reach. The graph's paths are OR: a node is reached through any one
way in. Episode 93's first step put recipes in that way, with each item a recipe takes as a way to
it, and the next step's test showed why that's wrong: a Bone Sword that took a Silver Ingot as well
as the sword and bones, when nothing drops ingots, passed, as the sword alone got a new player to
the recipe. And reaching can't tell a recipe that gives back more than went into it, round and
round, which makes items from nothing.

## Decision

- Recipes are content, in `content/recipes.txtpb`, with `recipes.proto`: an id, a name, the
  `inputs`, each an item and a count, the `output`, the same, the `seconds` it takes, and the
  `level` it needs. Every player knows each recipe of their level or below, so there's nothing to
  learn and nothing to save. `ParseRecipes` refuses a recipe with no level, no time, nothing to
  take, or none of an item, and `CheckContent` one that takes or makes an item the items file
  doesn't have, or more of one than go in a slot, so that what it makes always fits in a slot that
  what it takes has emptied. Three recipes make two new items, the Bone Buckler and the Bone
  Sword, and the Ash Blade, from a Bone Sword.
- A player crafts wherever they are. `World::Craft` starts a recipe if they're of its level, and
  their bag has all it takes, and room for what it makes once that's gone, or says why not. It
  takes from the bag, from the first slot on, and never from what's worn, nor from a place offered
  in a trade (`Trades::Holds`).
- A craft takes its recipe's seconds, while the player stands still. Moving, a walk they asked for,
  a swing or a skill, a blow, and so dying, a trade that opens, a duel they say yes to, and leaving
  stop it, and nothing's lost, as nothing's taken until it's made.
- Once the time's up, `World::Made` checks the bag again, the same check as at the start, in a copy
  of it: what the recipe takes is taken, and what it makes is given by `World::Give`, as loot is, so
  each one of what doesn't stack gets a serial and rolls of its own, and what was taken goes, serial
  and all. The copy goes into the bag whole, as one change, which the tick saves once. Each thing
  made is an event for the analytics, `craft`.
- The client sends `Input`'s `craft`, a `CraftAction`, to start or to stop, three at once and then
  one a second, and nothing starts while a world hands its players over. Each world a player gets
  has what they're making, `World`'s `crafting`, the recipe and how long they've been at it, which
  the client's bar fills from. K opens the crafting window.
- In the content graph, each recipe is a node, labelled `Recipe`. Each item it takes goes `INTO` it,
  which isn't a way a path follows, and it `MAKES` what it makes, which is. `AddContentGraph` ends
  in rounds: each asks smollerGraph for the items a new player reaches, in Cypher, by the shortest
  paths `Unreachable` asks for, and adds a `CRAFTS` from the new player to each recipe whose items
  they all are, until a round crafts nothing new. That's AND, for recipes, as a fixpoint over OR.
- `Cycles` finds each item that goes back into itself through recipes, with the way round, in two
  Cypher queries joined in C++: a shortest path from each recipe to each item it leads to, along
  `MAKES` and `INTO`, and each item `INTO` each recipe. An item whose recipe leads back to it goes
  round. The content tests expect none.

## Alternatives considered

- A forge, a workbench or an NPC to craft at, as most MMOs have. Each is another kind of thing in
  the content, a place in a land and a walk there, where episode 93 wanted crafting wherever a
  player is, between fights.
- Recipes as items to find and learn, and a crafting skill that levels, with quality tiers, and
  upgrades that can fail, from +1 to +N (left out of episode 93, to keep it small). Each is a set
  of what each player knows or has done, to save and send, and more rule. The level a recipe needs
  does the learning, with nothing kept.
- Taking from what's worn too. The sword in hand would go into a craft from under a fight, and a
  click on Craft would take what a player has on. Taking from the bag alone asks them to put it
  there first, which is one drag.
- Taking what a recipe takes as it starts, and giving it back if it's stopped (not weighed in an
  episode). Nothing could move it meanwhile, but every way a craft stops would have to give it
  back, and a crash or a handover in between would lose it, as decision 28 found for a trade. Taking
  it at the end, against the bag as it is then, needs nothing given back.
- Checking the bag only once the time's up. It's the check that counts, but a craft would run its
  seconds and only then say what was missing, so the start checks too, the same way.
- Each item a recipe takes as a way to it, as the graph had it in the episode's first step. It's OR,
  and the test with the ingot shows what it lets through.
- The rounds in Cypher alone. smollerGraph's Cypher reads, and has no `CREATE` to add the `CRAFTS`,
  so each round's reach is a query, and what it adds goes through the graph's own C++, by the
  builder that adds the rest.
- The cycle as one query, `MATCH (i:Item)-[:INTO|MAKES*]->(i)`. smollerGraph refuses one variable
  on two nodes, and a shortest path from a node to itself is the node alone, so it's a path from
  each recipe back to what went into it. A walk of our own over the recipes, in C++, would be about
  as long, and wouldn't say the way round in the graph's own words.

## Consequences

- Bones have a use. A skeleton drops 1.2 bones a kill on average, so the buckler's twelve take ten
  kills or so, and a Crypt Skeleton drops 2.4. Each recipe is fixed: the same bones always make
  the same item, where a drop is a chance.
- A craft holds a player still for its seconds, which a monster or an enemy at war can stop with one
  blow, so it's done somewhere safe, as players would.
- What a recipe makes is at most a slot's worth, so no recipe makes two swords, or sixty bones, at
  once.
- The bag's revision moves by more than one for a craft, as each change to the copy takes it on.
  Nothing counts the steps between two revisions: the bag changes once, and is saved once.
- The recipes are in `ClientHash`, as the window shows all of each, so a change to one needs a new
  client.
- A craft is the instance's memory, like a skill that's going on: after a handover, or a crash,
  the player's on the next instance with their bag as it was saved, making nothing, and has lost
  nothing.
- The graph's rounds are a shortest-path query each, three for the game's three recipes. A new
  kind of link that, like a recipe, needs all of what leads to it would go into the rounds too.
