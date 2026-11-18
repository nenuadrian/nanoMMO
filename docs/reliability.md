# Reliability

How reliable the game should be, how we measure it, what we do when it isn't, and what breaking it
on purpose taught us. Why we do it this way is in
[decision 39](decisions/0039-objectives-with-error-budgets-and-chaos-in-staging.md).

## Objectives and error budgets

A service level indicator is a number that says how the game is doing for players, as a share of
events that went well. A service level objective is how much of the time it has to be, over 30
days. What's left over is the error budget: how much may go wrong in a month before we've broken
the promise.

| Indicator | Good, out of all | Objective | Budget, a month |
|---|---|---|---|
| Ticks on time, each world | Ticks that ran within their 50 milliseconds, out of the 20 a second a world owes | 99.9% | 51,840 ticks, as long as 43 minutes frozen |
| Logins answered | Requests the login server answered within five seconds, a refusal such as a wrong password counting as answered, out of all | 99% | 1% of them |
| Saves fresh | Time with the saver's last good flush under 30 seconds old | 99.9% | 43 minutes, 12 seconds |

43 minutes is 0.1% of 30 days, from the SRE book's Appendix A, "Availability Table".

They come from what the servers measure: the ticks' histogram, `nanommo_tick_duration_seconds`, the
login server's `nanommo_login_duration_seconds` on port 9467, and the saver's
`nanommo_saver_flushed_timestamp_seconds`. A world that's down, or gone from Prometheus's targets,
though it was there in the last day, misses every tick, and a saver that's gone is stale. The 20 is
`kTicksPerSecond` in `server/main.cc`.

A world that stalls lets the ticks it missed go once it's more than a second behind
(`NextTick`, `server/tick.h`), so they're missed. Before episode 89 it ran them back to back
afterwards, which made up the count, and a world frozen for twelve seconds counted one tick missed.

### The rules

`monitoring/alerts.yml`'s `slos` group records each indicator's share of failures every 30 seconds:
each world's ticks over the last minute, and from those, over five minutes, 30, an hour, six hours
and three days, as `world:nanommo_tick_errors:ratio_rate5m` and so on, and the logins' and the
saves' over the same windows. `rate()` divides by its whole window, so a Prometheus that's just
started would read an hour's rate over its first minutes as a world that missed nearly every tick:
a world's minute counts once it has a minute's samples, and each longer window once Prometheus has
watched for half of it.

### Alerts on how fast a budget burns

A burn rate says how fast a budget's going: at 1, a month's lasts exactly the month; at 14.4, it's
gone in two days. As the SRE Workbook's chapter 5, "Alerting on SLOs", has it, each alert compares a
long window and a short one, a twelfth as long, so that it fires when the burning is real, and
stops soon after the burning does:

| Alert | Severity | Burn rate | Over | Spends of the month's budget |
|---|---|---|---|---|
| `TicksBurningBudget`, `LoginsBurningBudget`, `SavesBurningBudget` | page | 14.4 | an hour, and the last five minutes | 2% in the hour |
| the same | page | 6 | six hours, and the last 30 minutes | 5% in the six hours |
| `TicksDrainingBudget` | ticket | 1 | three days, and the last six hours | 10% in the three days |

A freeze of a few seconds burns fast over five minutes, but not over the hour, and pages nobody,
as `alerts_test.yml` checks. The logins' alert waits for ten logins in the hour, as a handful can't
tell a bad hour from bad luck. Each alert's value is its burn rate, and its severity a label; for
now Alertmanager mails them all to the same place.

The reliability dashboard, nanoMMO reliability, at http://localhost:3000/d/nanommo-reliability, has
each budget left this month, from what Prometheus has, the burn rates over the hour and the last
five minutes with lines at 6 and 14.4, the ticks, logins and saves as they go, the bots' world's
age, the players dropped, errors by service, the alerts firing, and each chaos experiment as an
annotation.

### When a budget's spent

A spent budget means the next episode is about reliability, whatever else was planned, until the
budget's back. A budget with plenty left is room for risk: a deploy, an experiment, a new feature.
With one maintainer, that's the whole policy.

## Chaos experiments

Chaos engineering, as the [Principles of Chaos Engineering](https://principlesofchaos.org/) put it,
is "the discipline of experimenting on a system in order to build confidence in the system's
capability to withstand turbulent conditions in production". An experiment says what the steady
state is, guesses what a fault will do to it, puts the fault in, and watches. `bench/chaos.sh` does
that, one fault at a time, in staging, while bots play ([Benchmarking](benchmarking.md#chaos-experiments)
has how to run it, and what its report says).

The principles also say to run experiments in production, as Netflix's Chaos Monkey does, killing
instances at random: each of their services runs many instances behind a balancer, so losing one
should go unnoticed. We run one instance of each world, on one server, so any kill there is an
outage for every player in it. We never break production on purpose, and the tool refuses a
deployment's stack. Staging is one machine, with bots, which says what our code does when Redis or
MySQL fails, not what a real network does.

### What the experiments found

With 100 bots in Ember, ten more logging in during each fault, and Frost, with nobody in it, as a
control. Before is 0.88.0, with the measurements and the tool and none of the fixes; after is
0.89.0, with all of them. The reports are JSON, and each has its trace ids.

| Experiment | Before | After |
|---|---|---|
| Redis frozen 5 s (`redis-pause 5`) | Every player in Ember froze, their world 5.9 s old at its worst. Ember ran the 100 ticks it owed back to back afterwards, so it counted 1 tick missed | With the timeouts alone (0.89.0's step 1), Ember crawled at 3 ticks a second instead, its players' world 0.7 s old, and the indicator counted 88 ticks missed. Failing fast too (step 2, measured with all the fixes): 4 ticks missed, the world 0.3 s old at its worst, nobody dropped |
| Redis frozen 12 s | Ember froze, and dropped all 101 players when Redis was back, as the leases had run out. 1 tick missed by the count. Back to steady 38 s later. [The postmortem](postmortems/2026-11-18-a-redis-pause-dropped-everyone-in-ember.md) | Ember ticked on, 10 ticks missed, its world 0.5 s old at its worst, and still dropped all 101 when Redis was back: their leases had run out in Redis meanwhile, which is what a lease does when Redis is gone for longer than it lasts. Open, in the postmortem |
| Redis stopped 15 s (`redis-down 15`) | The worlds ticked on, as each call failed at once, and Ember dropped all 101 once Redis was back. 28 logins failed | The same: Ember ticked on, and dropped all 101 once Redis was back. 29 logins failed |
| A trade while Redis is down, for 1.1 s | The giver left and kept the sword in Redis, the taker kept it too: one serial in two bags in MySQL, twice out of twice, and `ItemsDuplicated` fired | Called off: the save failed, so the world dropped both, unsaved, and each came back with their own sword. No serial twice |
| MySQL frozen 15 s (`mysql-pause 15`) | The worlds didn't notice: nobody could join, as the login server needs MySQL first, and it hung instead. It timed one login at 13.7 s, and the rest as quick | Each login failed after MySQL's three seconds, and then at once for five: the login server counted 43 failures, and `LoginsBurningBudget` paged. The saver fell 25 s behind, short of the 30 that counts against the saves. The worlds still didn't notice |
| MySQL 50 ms slower for a minute (`mysql-slow 60`) | Each join held Ember's thread between ticks, which the ticks' indicator can't see: no tick missed, and the bots' world 0.23 s old at its worst, from 0.06 | Much the same: 8 ticks missed, the world 0.27 s old at its worst. Loading players off the tick's thread is open |
| The saver killed mid-flush (`saver-kill 8`) | 88 bots' rows in MySQL left behind Redis's for good, while the saver said nobody was waiting, and its flushes were fresh | Nothing lost: the restarted saver put the batch back and saved it. No row behind |
| The login server frozen 20 s (`login-pause 20`) | Every login waited the twenty seconds, and the login server counted them all as answered in time | Each login gave up after five seconds, and the bots counted 3 timeouts, then logged in once the server was back. The login server's own metrics still saw nothing, which is why the bots count theirs |
| Consul stopped 30 s (`consul-down 30`) | Play went on; logins went through, with no worlds to list, and 54 lines of errors from the login server | The same for play. The login server now gives the worlds it listed in the last half a minute (not measured: the bots don't read the list) |
| Alloy frozen 60 s (`telemetry-pause 60`) | The worlds didn't notice, as the tracer never waits on its collector; 221 spans dropped | The same: 230 spans dropped, and nothing else |

Before the fixes, an hour of these spent almost none of the ticks' budget by the indicator, 100%
left to a tenth of a percent, though one of them dropped every player in Ember, and the logins'
budget three times over. The indicator measured the wrong thing, which is what 0.89.0's catch-up
cap is for.

Not every experiment found a bug. Frozen telemetry did nothing, as episode 87 meant, and saying so
is part of the result.

### What changed

- Redis answers each call within 250 milliseconds, and MySQL each read and write within three
  seconds, connecting within one, or the call fails. After a timeout, neither is asked again for a
  moment, a second for Redis, five for MySQL, so a tick pays for one Redis timeout a second, and
  what waits on MySQL for one each five seconds, rather than one each call. A connection that timed
  out is never used again, as its reply could come later, as the answer to another command.
  (`server/redis.h`, `server/database.h`)
- A world more than a second behind lets the ticks it missed go, rather than running them back to
  back, which played a stall over at many times the speed and hid it from the indicator.
  (`NextTick`, `server/tick.h`)
- The saver keeps each batch in Redis until MySQL has it, and the next flush puts back one that a
  saver died with. ([Saving players](saving.md))
- A trade Redis can't save is called off: both sides leave, unsaved, and come back with what Redis
  has, both bags from before it or both from after.
- A login gives up after five seconds, and the bots count theirs, by result, from outside.
- The login server gives the worlds it listed in the last half a minute when Consul doesn't answer.

### Known, and not fixed yet

- A join loads the player from MySQL on the world's thread, between ticks, so a slow MySQL holds the
  world, and the ticks' indicator can't see it, as the time isn't in a tick.
- A handover's hellos, a hundred at once, hold the next instance's thread for about a second, as
  episode 87 found, which the indicator can't see either.
- Redis is every world's single point of failure: one that's gone for more than a lease's ten
  seconds drops every player of every world with players, even with the fixes.
- A login reply that says the server couldn't answer looks like a refusal to the bots and the
  red-team client, which then try to make the account, and are told the name's taken.
- Nothing alerts on players dropped, and nothing watches the watchers: Prometheus, Alertmanager,
  Loki and Alloy can each be down with nobody told.
- No backups, so no restore drill.
- A session's outbox has no limit, so a client that stops reading grows the world's memory.

The [postmortem](postmortems/2026-11-18-a-redis-pause-dropped-everyone-in-ember.md) has some of these as its open action items.

## What we do of the SRE book, and what we don't

Google's [Site Reliability Engineering](https://sre.google/sre-book/table-of-contents/) is the book
most of this page's words come from. Most of it is about a large company running many services with
teams on call; nanoMMO is one maintainer and one server. This is where the game stands against it,
chapter by chapter, honestly, as of episode 89.

| Chapters | What it's about | What we have | What we don't |
|---|---|---|---|
| 3, 4: Embracing Risk; Service Level Objectives | How reliable is reliable enough, as a number | Three SLOs and their error budgets (episode 88). Risks we took on purpose before, such as dropping players whose lease ran out ([0031](decisions/0031-a-character-is-leased-to-one-instance-at-a-time.md)) | An agreement with players about them: they're our own goals |
| 5, 7: Eliminating Toil; The Evolution of Automation at Google | Doing by hand what a machine should | `deploy.sh` and `release.sh` (episode 19), `handover.sh` (36), deploying on push (37), `doctor.sh` (65), the Dependencies workflow, Flyway (18), `bench/chaos.sh` (88) | Rolling back is a command in [Deployment](deployment.md), by hand; runners' tokens and certificates are renewed by hand |
| 6, 10: Monitoring Distributed Systems; Practical Alerting | The four golden signals; alerting on symptoms | Latency (ticks, saves, stories, logins, the bots' world age), traffic (messages, bytes, players), errors (stderr in Loki, refused chat and stories, failed logins), saturation only as ticks a second out of 20; alerts on symptoms (`WorldFallingBehind`, `ItemsDuplicated`, the burn rates); promtool tests in CI; logs, profiles, traces. Prometheus descends from Borgmon, chapter 10's subject | No memory or CPU metrics of our own binaries; no exporters for Redis or MySQL; nothing watches the watchers: Prometheus, Alertmanager, Loki and Alloy can be down and nothing says so; no runbooks |
| 8: Release Engineering | Builds you can repeat, from pinned inputs | Pinned dependencies and image tags, builds and tests in Docker ([0004](decisions/0004-build-test-and-check-in-docker.md)), sanitizers, fuzzers and clang-tidy (28), versions and a changelog ([0020](decisions/0020-one-version-for-the-whole-game.md)) | The images are built again on the server, so what runs isn't the binary CI tested |
| 9: Simplicity | Less code, fewer moving parts | One thread for a world ([0008](decisions/0008-one-thread-for-the-world.md)), one server ([0016](decisions/0016-deploy-with-compose-to-one-server.md)), decision records | |
| 11, 13, 14, 16: Being On-Call; Emergency Response; Managing Incidents; Tracking Outages | People who answer, and how | Alertmanager mails every alert to `oncall@nanommo.example`; the objectives' alerts carry a severity, page or ticket | Nothing routes on the severity yet, and the mail goes to Mailpit unless `smtp_smarthost` says otherwise, so nobody is paged; one maintainer, no rotation, no incident roles, no log of outages. Chapter 13's "test-induced emergency" is what a chaos experiment is |
| 12: Effective Troubleshooting | Hypotheses, tested | Tracy and perf (8), Pyroscope, logs beside metrics, traces (87), the red-team client (72) | |
| 15: Postmortem Culture: Learning from Failure | Writing it down, blamelessly | [Postmortems](postmortems/README.md), from episode 89, after the book's Appendix D | Only the first |
| 17: Testing for Reliability | Tests up to disaster tests | Unit tests, the servers' tests against real MySQL and Redis (29), the load test (46), netem (9, 68), the red-team client, config tests (promtool, Loki's and Alloy's), and chaos experiments (88) | They're run by hand, in staging, not continuously |
| 19, 20: Load Balancing at the Frontend; in the Datacenter | Spreading players over servers | Worlds as shards a player picks (21), dungeon copies (38) | One machine |
| 21: Handling Overload | Saying no before falling over | Token buckets on chat, stories, items, guilds and duels; input queue caps; 64-kilobyte frames; each player sent what's near them | No admission control: a world takes whoever joins; a session's outbox has no limit |
| 22: Addressing Cascading Failures | Deadlines, failing fast, backing off | Consul on its own thread with timeouts; the storyteller's timeouts; the gateway's stale list; from episode 88, bots that back off with jitter, and from 89, timeouts on Redis and MySQL, failing fast after one, and a login's deadline | A join still loads the player on the tick's thread; Redis is shared by everything |
| 23: Managing Critical State: Distributed Consensus for Reliability | Agreeing on who owns what | Leases with fencing numbers ([0031](decisions/0031-a-character-is-leased-to-one-instance-at-a-time.md)) | One Redis, which saves to disk once a second, so after a crash a fencing number can go back; Consul runs in development mode, in memory |
| 24, 25: Distributed Periodic Scheduling with Cron; Data Processing Pipelines | Jobs that must run, and must finish | The saver and the recorder, and from episode 88 the saves' freshness as an SLO, and from 89 a saver that keeps its batch until MySQL has it | |
| 26: Data Integrity: What You Read Is What You Wrote | Not losing or corrupting data | MySQL as the truth ([0010](decisions/0010-mysql-as-the-source-of-truth.md)), transactions ([0028](decisions/0028-a-trade-is-checked-against-the-bags-and-saved-as-one.md)), serials and `ItemsDuplicated`, trades recorded once | No backups, and so no restore drill: MySQL, Redis and ClickHouse are volumes on one disk |
| 27: Reliable Product Launches at Scale; Appendix E | Launching carefully | The version rule, handovers without downtime (36) | No canary: a deploy hands every world over, one after the other, with no time to compare; no launch checklist |
| 28 to 32: Management | Teams | The getting-started page and the decisions | Mostly not ours, with one maintainer |
| Appendix A: Availability Table | What a number of nines allows | 99.9% of 30 days is 43.2 minutes, which is where the ticks' and the saves' budgets come from | |
| Appendix D: Example Postmortem | A postmortem to copy | [The template](postmortems/template.md) | |
