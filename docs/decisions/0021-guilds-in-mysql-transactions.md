# 21. Guild changes as MySQL transactions, and invitations in Redis

## Status

Accepted

## Context

Episode 66 gave the game guilds, the first thing one player changes for others: a leader who
leaves makes someone else the leader, and in episode 67, an officer removes a member. Until then,
each player was saved by their own world, into Redis
([0013](0013-save-players-through-redis.md)), as nobody else changed them.

## Decision

- Guilds are in MySQL alone, from V11: `guilds`, with a unique key on the world and the name,
  which MySQL's default collation compares without case, and `guild_members`, keyed by the
  character, so a character is in one guild at most.
- Each change is a transaction. Founding inserts the guild and its leader together. Every other
  change locks the guild's row first, `SELECT ... FOR UPDATE`, then the member's (`LockFor` in
  `server/guilds.cc`), so changes to one guild happen one at a time, always in that order.
- A statement waits two seconds at most for a lock, and fails then as `Aborted`, and the player is
  told to try again.
- Invitations are in Redis, as `guild_invite:<character>`, set with `SET ... NX EX 60`, so a
  player is asked by one guild at a time, from any instance, and the ask runs out by itself.
- The roster is read from MySQL once a tick at most for each guild, and each world adds who's
  playing there, and their level as it is (episode 67).

## Alternatives considered

- A guild in Redis, with its players. Any world that had it could write it back stale (episode 66).
- A check in our code that a name is free. Two players founding one name at once, on two
  instances, could both pass it, where MySQL's unique key refuses the second (episode 66).
- Invitations in MySQL, with an expiry column and a job to clear them out, where Redis deletes a
  key for us when its time is up (episode 66).

## Consequences

- Locking in one order can't deadlock. A test runs a kick and a leave at once, on two connections,
  and whichever goes second finds the player gone (episode 67).
- A guild action runs in the world's tick, which waits for MySQL
  ([0008](0008-one-thread-for-the-world.md)). MySQL's own wait for a lock is fifty seconds, which
  would freeze the world, so ours is two, still a very long tick.
- The roster's levels come from MySQL, a few seconds behind the game, but for the players online
  in the world, whose levels it has as they are.
- An invitation outlives a failed join: joining deletes it only once the player has joined or
  can't, so a MySQL that fails for a moment leaves it to try again.
- Deleting a character takes its membership with it, and a guild's rows go with the guild.
