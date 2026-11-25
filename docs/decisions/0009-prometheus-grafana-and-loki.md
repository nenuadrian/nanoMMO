# 9. Metrics, logs and alerts with Prometheus, Grafana and Loki

## Status

Accepted

## Context

Episode 1 planned for monitoring from the start, which meant deciding what's worth watching, such
as how long each tick takes and how many players are online, and using an open format, so that the
game isn't tied to one provider's tools. Episode 4 served the first metrics, episode 30 added
alerts, and episode 35 the logs.

## Decision

- Each world serves its metrics in Prometheus's text format, over HTTP, with prometheus-cpp.
  Prometheus pulls them every two seconds, and finds the worlds in Consul
  ([0018](0018-find-the-worlds-through-consul.md)).
- Durations are histograms, so the dashboards show the p50 and the p99, as an average would hide
  the slow ticks among the fast ones (episode 4).
- Grafana draws them, with its data sources and dashboards provisioned from files in
  `monitoring/`, so they come up the same on any machine, and change through git.
- Prometheus checks the rules in `monitoring/alerts.yml`, and Alertmanager emails each alert,
  through Mailpit while developing. The rules have tests, which CI runs (episode 30).
- The servers write their errors to stderr, and the rest to stdout. Alloy collects every
  container's lines into Loki, which indexes their labels and not their text, Grafana shows them
  next to the metrics, and Loki's own rules alert on them (episode 35).

## Alternatives considered

- A cloud provider's own stack, such as CloudWatch on AWS, or Azure Monitor. Each integrates with
  Prometheus anyway, and an open format leaves the game free to move (episode 1).
- Pushing metrics to a gateway, which suits a short job that ends before a scrape. Our servers run
  for hours, so Prometheus pulls, and a server needn't know where Prometheus is (episode 4).
- A log store that indexes what each line says. This is the obvious alternative, and it wasn't
  weighed; indexing the labels alone keeps the index small and logs cheap to keep.

## Consequences

- Each world serves its metrics on a port of its own, which it registers with Consul, as two
  worlds on one machine can't share one (episode 21).
- A world that stops leaves Consul, and Prometheus stops scraping it, so `up == 0` misses it.
  Episode 30's first `WorldDown` resolved with the world still down, and the rule now looks for a
  world that was a target in the last day.
- The saver keeps going while MySQL is down, and nothing in the metrics shows it, so a rule on its
  logs, `SaverFailing`, does (episode 35).
- Alloy reads Docker's socket, which gives it as much power as root on the machine, to read logs.
- On a server, Prometheus, Grafana, Loki and Alertmanager listen on its localhost, and we reach
  them through SSH ([Deployment](../deployment.md)).
- The same stack took more in later, with the version, as `nanommo_build_info` (0.59.0), and
  Pyroscope's profiles all the time (episode 46). The game's analytics went to ClickHouse, a store
  of their own (episode 47).
