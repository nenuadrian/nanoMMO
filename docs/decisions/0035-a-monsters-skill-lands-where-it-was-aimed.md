# 35. A monster's skill lands where it was aimed, after a telegraph players can step out of

## Status

Accepted

## Context

Until episode 83, a monster only swung at the player it was fighting, a blow that lands 0.65
seconds in if they're still within its 1.2 metres. Nothing to see coming, and nothing to do but
trade blows or run. Episode 84 brings a world boss, and a boss's fight is about its big attacks.
Each shows where it will land, for long enough to get out of the way. Episode 83 gives the Ash and
Crypt Skeletons one each, the Ground Slam, a circle three metres round the monster, and the Bone
Sweep, a cone 90 degrees wide and four metres long, to build that on.

Stepping out only works if what's checked as the skill lands is what the player saw as it started,
and the player sees it through the network. Their keys reach the server half a round trip late,
and the world that says it's landed reaches them half a round trip after it has.

## Decision

- A monster's skills are content (`content/monsters.proto`'s `Skill`, in `Kind.skills`). Each has a
  shape, `CIRCLE` or `CONE` with its `arc`, a `size`, a `telegraph_ms`, a `damage`, a `cooldown_ms`
  and a `range`. A monster uses the first that's ready, and within whose range the player it's
  fighting is, in place of a swing.
- The shape is fixed as the skill starts, to where the monster stands and the way it faces
  (`Monster.cast_x`, `cast_z`). A push moves the monster and not the shape. A throw calls it off.
- As it lands, the world hits each living player in the shape where they are on the server then
  (`World::LandSkill`, with `InShape` from the content's library, which the bots use too).
- The content refuses a telegraph shorter than half a second (`kShortestTelegraph`), a cooldown
  shorter than the telegraph, and a range past the shape, so that whoever the skill starts on is in
  it.
- While it's under way, each world sends the skill's id and how long until it lands, in
  milliseconds (`Monster.skill`, 12, and `lands_in`, 13). The client draws the shape where the
  monster was as it first saw the skill, and counts down by itself between worlds
  (`client/telegraphs.h`).

## Alternatives considered

- A shape that follows the monster, turning to face the player, as a swing does. Nobody could step
  out of it, so it would be a slower swing that hits harder.
- Hitting whoever's in the shape as it starts, and showing who'll be hit. Moving would do nothing.
- Checking where each player's own screen had them as it lands, as a duel's blows are checked where
  the attacker saw the target ([0034](0034-duel-blows-are-checked-where-the-attacker-saw-the-target.md)).
  A player's screen has them ahead of the server, from the keys it's sent, so it would mean
  believing where a client says it is.
- Filling the shape a round trip sooner on each screen, so that it ends as the last keys that count
  leave (not weighed in an episode). Players would see it land before it hurt them, and the shapes
  on two screens would end at different times.
- When it lands on the server's clock, which stays the same through the skill, so that the world
  could leave the monster out while it doesn't move ([0024](0024-leave-out-what-has-not-changed.md)).
  The world would need the time on the server's clock, which it doesn't keep, for each world it's
  in, and the client would have to work out the server's clock from the interpolation's. A few
  bytes a tick for a monster under way is cheap.
- A count of the skills each monster has started, as `Player.casts` has, to tell the next skill from
  the last. The time left does it. A skill only counts down, so one with more left than a late world
  could say is the next.

## Consequences

- Fights with Ash and Crypt Skeletons are about where you stand. Walk out of a slam in time and it
  misses, stay and it takes about a quarter of your health
  (`World.StepOutOfTheCircleAndItMisses`, `World.StayInTheCircleAndItHits`).
- A player has to be out a round trip before the fill ends on their screen. The round trip is 50 to
  200 milliseconds of the two seconds the slam shows, and of the second and a half the sweep does.
- A monster knocked out of its own shape still hits whoever stayed in it
  (`World.ASkillLandsWhereItWasAimed`).
- A monster under way is sent again each tick, as one that moves is.
- A client that first sees a monster partway through a skill, after a push, draws the shape where
  the monster is then.
- The clients draw the shapes, so changing a skill's shape, size, arc, telegraph or colour needs a
  new client, as each is in the `ClientHash`. Its damage, cooldown and range don't.
- A boss is a kind with several skills, in the order it tries them; a rotation and phases are
  still to come.
