# 39. Objectives with error budgets, and chaos experiments in staging, never in production

## Status

Accepted

## Context

By episode 87, the game had metrics, logs, profiles and traces, and alerts on a few symptoms: a
world falling behind or down, an item held twice. None said how reliable the game should be, or how
far it was from that, and nothing had tested what it does when what it depends on fails. Episode 79
froze a world for eleven seconds, on purpose, once, to see its players dropped; that was all.

In episode 88 we broke things on purpose, one at a time, with bots playing, and found:

- Redis frozen for five seconds froze every player in Ember with it, as each tick waited for Redis,
  and the indicator we had, ticks a second, barely moved: the world ran the hundred ticks it owed
  back to back afterwards. Frozen for twelve seconds, past the leases' ten, it dropped all 101 of
  them, and counted one tick missed.
- A trade during a Redis outage that lasted 1.1 seconds made a copy of a sword, twice out of
  twice.
- A saver killed mid-flush left 88 players' rows in MySQL behind Redis for good, while the saver
  said nobody was waiting.
- The login server frozen for twenty seconds made every login wait twenty seconds, and its own
  metrics counted each as answered in time.
- MySQL frozen for fifteen seconds cost the worlds nothing, as nobody could join without the login
  server, which needed MySQL first. Alloy frozen for a minute cost them nothing either.

## Decision

- Three service level objectives, over 30 days, in `monitoring/alerts.yml`'s `slos` group: 99.9% of
  each world's ticks within their 50 milliseconds, of the 20 a second it owes; 99% of the requests
  to the login server answered within five seconds, a refusal such as a wrong password counting as
  answered; MySQL no more than 30 seconds behind Redis 99.9% of the time. What they leave over is
  an error budget: 51,840 ticks a world, 1% of the logins, 43 minutes of stale saves a month.
- Recording rules work each one out every 30 seconds, ticks from 1-minute windows, and longer
  windows from those, each once Prometheus has watched for half of it. Alerts watch how fast each
  budget burns, as the SRE Workbook's chapter 5, "Alerting on SLOs", does: a page at 14.4 times over
  an hour and the last five minutes, or 6 times over six hours and the last 30 minutes, and a ticket
  at once over three days and the last six hours. The reliability dashboard shows each budget left.
- A spent budget means the next episode is about reliability, whatever was planned.
- Chaos experiments run in staging, on the Compose stack on one machine, with `bench/chaos.sh`: one
  fault at a time, with a steady state checked before and after, a fault that takes itself out, and
  a report of what the metrics, logs and traces saw. It refuses a deployment's stack. We never break
  production on purpose.
- What an experiment finds that matters, such as a dupe or players dropped, gets a postmortem, in
  `docs/postmortems/`, blameless, after the SRE book's Appendix D.

## Alternatives considered

- Availability as `up`: the share of scrapes a world answered. A world frozen on Redis answers its
  scrapes from another thread, so it was up for all twelve seconds of the experiment above.
  Players care about ticks, not about the metrics endpoint.
- Ticks a second, as `WorldFallingBehind` has it, as the indicator. A world that runs the ticks it
  missed back to back afterwards makes up the count, which is why a stalled world lets them go now
  (episode 89, step 1).
- Alerts on thresholds alone, such as more than 5% of ticks late over five minutes. They page for a
  blip that spends almost nothing of the month's budget, and stay quiet for a slow leak that spends
  all of it. A burn rate over two windows does neither.
- The logins' indicator from Tempo's span metrics, which the traces give us already. They'd make
  the objective depend on Alloy and Tempo, which the telemetry experiment takes down, and Alloy
  keeps a tenth of the quick traces. The login server counts its own.
- Netflix's Chaos Monkey, which kills instances in production at random. It works because each of
  their services runs many instances behind a balancer, so losing one should be invisible, and the
  monkey checks that it is. We run one instance of each world, on one server, so any kill is an
  outage for every player in it. The Principles of Chaos Engineering say to run experiments in
  production; we take their other advice, a hypothesis about the steady state, real faults, a small
  blast radius, and automation, and run them in staging.
- Pumba, which pauses, kills and delays containers from a container of its own. It does what
  `docker pause`, `docker kill` and netem do already, with one more image.
- Toxiproxy, a proxy that adds latency and faults between a service and what it calls. It changes
  the network it tests: each server would talk to a proxy rather than to Redis or MySQL.
- Chaos Toolkit, which describes experiments in JSON and runs them. Each of ours is a few lines of
  shell around `docker`, and a framework would be more to learn than to write.

## Consequences

- An outage now costs something we can count: a world frozen for twelve seconds misses 240 ticks of
  its 51,840. A blip doesn't page, and a month's worth of blips does.
- Each objective is measured from inside, by the servers, apart from the bots' logins, which count
  from outside, as a player does. A request that never reaches the login server, or never ends, the
  server never counts.
- The experiments run by hand, and in staging, which is one machine with nothing else on it. They
  say what our code does when Redis or MySQL fails, not what a real network does.
- Rules over windows of up to three days, on a Prometheus that keeps 31, take more of its memory
  and disk than the alerts before.
