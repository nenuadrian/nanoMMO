# 36. A boss fights whoever's hurt it most, and its death is everyone's who took 5% of its health

## Status

Accepted

## Context

Episode 84 brings the Bone Colossus, a world boss, with 6000 health, for a crowd to take on
together. Until then, a monster went for whoever hurt it last, and its death was the killing
blow's. Its experience, its loot and the kill for a quest went to that player alone. With one
player on a skeleton, that's fair. With twenty on a boss, it isn't.

- A boss that turns to each new blow faces nobody for long. Its slams and sweeps would be aimed
  at whoever happened to hit it last, a few times a second, and nobody could hold it, or keep it
  facing away from the rest.
- A fight of two minutes would give one player of twenty its experience and its loot, whoever
  landed the blow that took its last health. Everyone else's two minutes would count for
  nothing, and a player who'd done nothing could take it with one blow at the end.

## Decision

- Each monster keeps a threat table (`Monster.threat`), where what each blow takes from its health
  is threat for the player who struck it (`World::Hurt`). It goes for whoever has the most, seen or
  not, while they're alive and within its leash, and on a tie, it keeps to the one it's fighting
  (`Spot`, in `server/monsters.cc`). With one player on a monster, that's whoever hurt it last,
  as before.
- Once nobody who's hurt it is left within its leash, a monster forgets them. A boss goes home
  then, back at full health and in its first phase, and nothing hurts it on its way, until it's
  there (`Monster.evading`).
- A boss's death is everyone's who took at least 5% of its health (`World::kEnoughHelp`). Each
  gets all of its experience, a roll of its loot of their own, and a kill for their quests, those
  who died in the fight too. A monster that isn't a boss is still its killer's alone.

## Alternatives considered

- Going for the nearest player, as a monster does before it's hurt. The boss would go for
  whoever stood closest, and a crowd round it would make that whoever moved last.
- A threat table where a player has to take more than the one the boss is fighting by a margin,
  such as a tenth more, before it turns, as some games have (not weighed in an episode). It would
  keep a boss from turning between two players who are close, but a tie already keeps it on the
  one it's fighting, and the margin is a number to tune once there are players who heal, or
  taunt, whose threat isn't damage.
- The experience shared between the helpers, by what each took. Twenty players would get a
  twentieth each, so a boss would be worth less to each than a skeleton, and nobody would come.
- One roll of the loot, for whoever took the most, or handed round. One player of twenty would
  get anything, and handing it round needs a party, which the game doesn't have.
- Credit for any blow at all. A player passing could hit it once and take the same as those who
  fought it for two minutes.

## Consequences

- A crowd can take a boss on together, and each who helped gets something for it, as 5% of the
  Bone Colossus is 300 health, thirty to forty swings of a new player's sword, which a player who
  fights it for a minute takes
  (`World.EveryoneWhoHelpedGetsALootRoll`, `World.ALittleHelpIsntEnough`).
- The player who's doing the most is the one the boss turns its slams and sweeps on, so those
  round it can step round behind (`World.TheBossFightsWhoHurtItMost`).
- Threat lasts for the fight. A player who dies keeps theirs, and their share, until the boss
  dies or goes home, and one who isn't in its land as it dies gets nothing.
- Each kill of a boss gives out as many rolls of its loot as there were players who helped, so
  its gold and its drops come into the game twenty at a time, and the Bone Colossus's 40 to 90
  gold goes to each.
- A boss can't be fought from past its leash, as it goes home, healed, as soon as nobody who's
  hurt it is within it (`World.TheBossGoesHomeWhenNobodysLeft`). A skeleton still can be, as
  before, from a skill that reaches past its four metres.
