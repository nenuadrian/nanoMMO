# 33. Players fight each other in duels, which end at one health

## Status

Accepted

## Context

Until episode 81, every blow in the game was a player's on a monster or a monster's on a player.
A player picked a monster, `Player.target`, by its id, and `World::Land` hurt the monsters a skill
reached. A player with no health died, waited ten seconds and came back in the middle of their
land, at full health, and the dead are saved as they'll come back
([0013](0013-save-players-through-redis.md)): in the middle, at full health.

Episode 81 is the first where players fight each other, and it had to settle three things: who may
hit whom, what a blow that would kill does, and how a client says which player it means, as
players' ids are their characters' in MySQL, and monsters' a world's own count from one, so the
same number can be both.

## Decision

- A player hurts another only in a duel that both have said yes to, which a World keeps in
  `Duels` (`server/duels.h`), as both are in its copy of the land. One asks, by name, within ten
  metres; the other says yes within thirty seconds; and after a countdown of three seconds,
  `Duels::Opponent` names each side's opponent. `World::Land` hurts that player, and no other,
  besides the monsters.
- The blow that would leave a duellist no health leaves them one, and they've lost
  (`World::Hurt`, for a player). Nobody dies in a duel, nothing drops, and it gives no experience.
  A duellist who goes more than 25 metres from the flag, where the one who asked stood as the
  other said yes, or who dies, to a monster, loses too.
- Health isn't given back at the end. It comes back as after a fight with monsters, from a potion,
  a skill or a new level.
- The player picked goes beside the monster: `Player.target_player`, which `Input.target_player`
  (20) sets with each input, and `protocol::Player.target_player` (16) shows while a skill's used.
  `World::Pick` keeps it only for the opponent in a duel that's started, in place of the monster,
  so picking anyone else picks nobody. `World::Target` returns where the one picked is, monster or
  player, which the run-up and the aim go by.

## Alternatives considered

- A duel to the death, through the death and the respawn there are (the obvious one, not weighed
  in an episode). The loser would wait ten seconds and wake in the middle of the land, far from
  where they fought, and come back at full health, as the dead are saved, so losing a duel would
  heal them. A death would count as one in the analytics too, beside the monsters' kills.
- Giving both sides their health back at the end, as many games do. A duel would then be a full
  heal for anyone a friend will duel and yield to, after a fight with monsters.
- One target for both, with players and monsters told apart by a flag or a range of ids. It would
  be a rule about numbers that both sides of the wire have to keep, where a field of its own says
  what it is, and costs nothing while it's zero, as protobuf leaves it out.
- Players hurting each other anywhere, or in some lands, with the rules that go with it: who may
  attack whom, what a death costs, and what stops a player hunting the newest (not weighed: it's for
  a later episode, if the game has it).

## Consequences

- Nobody can be hurt by another player without saying yes, and a client that picks someone who
  isn't its opponent picks nobody, which `Duels.OthersCantHitADuelist` checks.
- A duel is between two players in one copy of a land, so going through a gate, leaving the game,
  or the world handing its players over to its next instance calls it off, with nobody winning.
- A duel's blows don't push or throw a player, as their own keys move them, and the client's fx
  hold still for a blow that lands on a monster, not on a player.
- The client keeps both targets, and `Picked` in `client/picking.h` says which figure is the one
  picked, for the ring, the frame, the hotbar and the fx. The compass and the map mark a monster
  picked, and not a player.
- A duellist is hit where the server has them as the blow lands, while the other's client draws them
  100 milliseconds in the past ([0023](0023-draw-everyone-else-in-the-past.md)), so against a player
  who's moving, a blow that looked as if it missed can land, and the other way round. Lag
  compensation is for the next episode.
- Monsters fight on during a duel, so a monster that kills a duellist ends it, and they're dead, as
  they would be outside one.
