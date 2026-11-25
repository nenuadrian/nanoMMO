# 31. A character is leased to one instance at a time

## Status

Accepted

## Context

A world runs as two instances while it hands its players over to the next one
([0016](0016-deploy-with-compose-to-one-server.md)), and each instance knew only its own players.
`Server::HandleHello` turned away a character who was playing on that instance, but nothing stopped
one playing on two, each with a token from the login server. Each instance loaded the character
from Redis, and saved them as it had them ([0013](0013-save-players-through-redis.md)), over what
the other saved.

In episode 79 the red-team client ([0025](0025-the-server-checks-what-a-client-asks-for.md)) found
it, with `twice`, where mordred came into Ember on the instance on port 7777 and on the one on 7877,
traded its sword to morgan on the first and left it, which saved its bag without the sword. On the
second, which had loaded the bag before the trade, it moved the sword into the bag, a change that
instance saved on its next tick, over the trade. It came back with the sword, and morgan had one
too, which made three swords from two.

Decision 28 named what was still trusted, that one server saves a player at a time, and
that an item's copy can't be told from the item. Nothing kept a record of trades either.

## Decision

- A character plays on one instance of its world at a time, which holds their lease. The lease is a
  key in Redis, `playing:<world>:<character>`, holding the instance's id, `InstanceId`, its world's
  and its port's as in Consul, and a fencing number from the world's counter, `fencing:<world>`,
  which is higher for each lease. A lease reads `Ember-7777 42` (`server/players.h`).
- `Server::HandleHello` takes the lease before it loads the player, and turns them away if another
  instance has it, with `refused player 7: they're playing on Ember-7777`. A lease lasts ten seconds
  (`Players::kLease`), and `Server::RenewLeases` renews all of an instance's leases each second,
  in one round trip. `Server::Leave` saves the player and lets go of their lease.
- `Server::HandOver` saves its players, hands each lease to the next instance, with a new number,
  and only then tells the clients to move. An instance takes a lease handed to it as its own.
- Taking, renewing, letting go and handing over are Lua scripts, which Redis runs with nothing in
  between, so each reads the lease and writes it as one, and touches only a lease that's still the
  one the instance took. A refused hello takes no number.
- The leases fence the saves. `Players::Save` writes its players only if each one's lease is still
  the one the instance took, instance and number, which `Redis::TransactionIf`'s script checks
  before it runs the save's commands, all in one round trip, and runs none of them if one isn't.
  An instance that finds a lease isn't its own any more, as it saves or renews, drops the player
  without saving them (`Server::Drop`). A trade whose save is refused drops both its players.
- Each item that doesn't stack has a serial, `items.Stack.serial`, 64 random bits from
  `std::random_device`, from `World::Give`, the kit, and, at a join, for an item saved before
  serials.
- Each trade that's done has a record, with an id from `RandomHex`, who traded and what each gave,
  in the `trades` stream, in the same script as its bags. The saver takes each, with both its
  players, in one batch, into the `trades` table from `V13__trades.sql`, in the same transaction as
  the bags, with `INSERT IGNORE`, so a batch tried again records it once.
- The saver counts the serials that more than one item in MySQL has, and serves the count as
  `nanommo_duplicate_items`, on port 9466. `ItemsDuplicated` fires once it's been above zero for a
  minute (`monitoring/alerts.yml`).

## Alternatives considered

- A lease without the fencing, as the episode had it first. An instance that stalls for longer
  than a lease, a process stopped or a machine that's swapping, loses it without knowing, and once
  it goes on, it saves the player over what the instance that took them saved. Episode 79 stopped
  one with `SIGSTOP` for eleven seconds to show it.
- Checking the leases with `WATCH`, `MULTI` and `EXEC`, from decision 28. It's two round trips, as
  the leases have to be read before the transaction, and an `EXEC` that Redis aborts, as a lease
  changed, doesn't say which one, so the instance can't tell whom to drop.
- A fencing number that each write compares against the highest it's seen, as a storage service
  that keeps its own would. Redis has the lease itself, so a save checks that the lease is still
  the instance's, number and all, which is as strong, and needs nothing else kept.
- A lock in MySQL, a row for each character playing, as guilds are changed
  ([0021](0021-guilds-in-mysql-transactions.md)). The saves go into Redis, which a lock in MySQL
  can't fence, and a world's tick would wait on MySQL.
- The second instance taking the character over, and the first dropping them, last one wins (the
  obvious alternative, not weighed in an episode). It's a lease too, but one that `twice` takes
  whenever it likes, with the first's unsaved changes lost each time.

## Consequences

- A hello is a Redis round trip more, a save is a Lua script in place of a transaction, still in
  one round trip, with a read of each lease in it, and each second one more script renews all of an
  instance's leases. `Redis::Transaction` stays, for whatever needs no check.
- A character whose instance crashes can't play on another for up to ten seconds. One whose
  instance starts again on its port plays at once, as it's the same instance by its id.
- An instance that stalls for ten seconds loses all its leases, taken or not, and drops those
  players as it goes on. A refused trade drops a player whose lease was fine, who comes back as
  Redis has them, from before it.
- The id is the world and the port, so two instances of one world on two machines, on the same
  port, would share an id. The game runs on one machine ([0016](0016-deploy-with-compose-to-one-server.md)).
- Some things are still trusted. A trade whose save fails, as when Redis can't be reached, is only
  in memory until the next save. If the instance loses one side's lease in between, the other side
  is saved with what they got. And a trade that comes in while the saver reads a batch can have one
  side in MySQL a flush before the other, which `ItemsDuplicated`'s minute rides out.
- MySQL's `trades` table says who gave whom each item, by its serial, for whoever puts a dupe
  right.
