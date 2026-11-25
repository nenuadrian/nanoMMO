# A Redis pause on 2026-11-18 dropped everyone in Ember, and the tick SLO said one tick was missed

## Status

Done, 2026-11-18, in episode 89.

## Summary

In a chaos experiment in staging, Redis was frozen for twelve seconds. Every player in
Ember froze with it, and when Redis came back, Ember dropped all 101 of them, without saving them.
The indicator we'd just made, ticks on time, counted one tick missed, and no alert fired.

## Impact

101 players, the bots in Ember, frozen for about thirteen seconds, then disconnected.
They lost no more than a tick's progress, as each was saved in the tick before Redis froze, but it
took 38 seconds for all of them, and the ten who came to log in meanwhile, to be back in. Logging in
waited for Redis too, and one login took 10.95 seconds. Frost, which nobody was playing, ticked on,
as a world with nobody in it has nothing to save. By the indicator's count, Ember spent one tick of
its ticks' error budget, out of 51,840. By what happened, it spent 260, the thirteen seconds it was
frozen.

## Root causes

The code assumed Redis always answers, and soon. A world's tick saves its players
and renews their leases in Redis, and its calls had no timeout, so the tick waited as long as Redis
did, and every player in the world waited with it. A lease lasts ten seconds, so once Redis had been
gone for longer, the leases had run out, and the world's first save after it came back found none of
its players its own, and dropped them, as [decision
31](../decisions/0031-a-character-is-leased-to-one-instance-at-a-time.md) has it. And a world that
had stalled ran the ticks it owed back to back once it could, all 260 of its thirteen seconds, each
well within its 50 milliseconds, so the ticks' indicator counted them as on time.

## Trigger

`bench/chaos.sh redis-pause 12`, which froze Redis's container with `docker pause`, as
a Redis that hangs looks from outside, such as on a swap storm, a slow disk under its append-only
file, or a `KEYS *` someone runs on it.

## Resolution

The experiment thawed Redis after twelve seconds. The bots logged in again by
themselves, after a wait that doubles, with jitter, five a second at most.

## Detection

Nothing. `WorldFallingBehind` was pending, and never fired, as it needs a minute.
The drops went to the world's standard output, not its errors, and nothing alerts on
`nanommo_players_dropped_total`. The ticks' burn-rate alert couldn't see the freeze, for the reason
above. We knew because we were watching.

## Action items

| Action item | Kind | Done in |
|---|---|---|
| A timeout on each Redis call, 250 milliseconds, so no call waits longer than that | mitigate | 89, step 1 |
| After a timeout, don't ask Redis again for a second, so a tick pays for one timeout a second | mitigate | 89, step 2 |
| A world more than a second behind lets the ticks it missed go, so the indicator sees a stall | detect | 89, step 1 |
| An alert on players dropped without saving | detect | open |
| A lease that ran out while Redis itself was gone, and that no other instance has taken, is taken again rather than its player dropped | prevent | open |
| Load and save players off the tick's thread, so a slow Redis or MySQL slows a join, not the world | prevent | open |
| A second Redis, a replica that takes over, so that one Redis isn't every world's single point of failure | prevent | open |

## Lessons learned

### What went well

- Frost, the control, ticked on, which showed it wasn't the machine. It couldn't show it was Redis,
  as a world with nobody in it never calls Redis.
- The bots came back by themselves, spread over half a minute rather than all at once, which they
  couldn't before episode 88.
- Nothing was lost but the time, as each player had been saved in the tick before.
- The chaos tool took the fault out by itself, and said what the metrics, logs and traces saw.

### What went wrong

- The indicator meant to tell us about this exact failure said all was well.
- Nothing alerted. The drops weren't even errors in the logs.
- A twelve-second hang anywhere in Redis turns into everyone in every world with players dropped,
  as every world's tick waits on the one Redis.

### Where we got lucky

- It was staging, and bots.
- Nobody was in the middle of a trade. The same kind of outage, a second long, during a trade, copies
  the item traded (fixed in 89, step 4).

## Timeline

From the chaos report, `bench/chaos.sh redis-pause 12`, at 0.88.0, from when Redis froze.

| Time | What happened |
|---|---|
| −2 s | Steady, with Ember and Frost at 20 ticks a second, 100 bots in Ember, their newest world 60 ms old |
| 0 s | Redis frozen. Ten more bots start logging in, over the next twelve seconds |
| 0–2 s | Ember's tick waits on Redis, at 12 ticks a second, then none |
| 2–13 s | Ember runs no ticks. Its players' newest world grows older, 10 seconds by 12 s. Frost runs its 20 a second |
| 10 s | The leases taken or renewed just before the freeze run out |
| 13 s | Redis thawed. Ember runs the 260 ticks it owed back to back; its first save finds none of its leases, and drops all 101 players |
| 18 s | 3 bots connected, the ten new ones logging in |
| 22–46 s | The bots log in again, five a second at most |
| 51 s | Steady again, 38 seconds after the thaw |

## Supporting information

- In the report, Ember owed 1,040 ticks in the window, ran 1,040, and 1,039 within 50 milliseconds;
  101 players dropped; the bots' newest world over ten seconds old at its worst; the slowest trace a
  `log in` of 10.95 seconds, `a3db877bafeb30a08fb3e8ad558f0122`; no traces with errors; nothing on
  standard error but the storyteller's, which the stack didn't run.
- The same experiment after the fixes is in
  [Reliability](../reliability.md#what-the-experiments-found).
