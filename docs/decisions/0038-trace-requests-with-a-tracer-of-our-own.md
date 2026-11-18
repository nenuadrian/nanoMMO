# 38. Trace requests with a tracer of our own, which speaks OpenTelemetry

## Status

Accepted

## Context

By episode 86, the servers had metrics in Prometheus ([0009](0009-prometheus-grafana-and-loki.md)),
their logs in Loki, profiles in Pyroscope, and Tracy for the tick. Each says how one process is
doing. None follows one request through the processes it goes through: a player entering a world
goes from the login server, through Redis and MySQL, to a world, and a trade from a world, through
the trades stream, to the saver. When entering a world got slow, the metrics could say that it had,
and not where the time went, or in which process.

In episode 87 we traced them, and the traces showed it the first time we looked: each statement to
MySQL took two round trips, a prepare and a run, and a hello four statements, all on the world's
thread, which ticks. With MySQL's replies delayed by two milliseconds, a hello took 19 milliseconds,
and a handover of a hundred players kept the next instance from ticking for 1.95 seconds.

## Decision

- The login server, the worlds and the saver trace the work they do for a request, with
  `server/tracing.h`: each request is a trace, and each piece of its work a span, a server span for
  the request, an internal one for a piece of its work, such as checking a password, and a client
  span for each call to Redis, MySQL or Consul made for it. The requests are the login server's,
  a player's hello, a trade and a handover.
- A trace goes from one process to another in what carries the request already, as a W3C
  `traceparent`: in the token's record in Redis, from the login server or a world handing its
  players over to the world the player says hello to, and in a trade's record on the trades
  stream, to the saver.
- Spans go to the collector that `NANOMMO_OTLP_ENDPOINT` names, in OpenTelemetry's protocol, as
  JSON over HTTP, from a thread of the tracer's own, through a queue that drops what it can't hold
  and counts it. Unset, a server traces nothing.
- Alloy takes the spans, keeps every trace with an error, every one over 100 milliseconds and a
  tenth of the rest, deciding once a trace is whole, and sends them to Tempo, which Grafana reads,
  with links from a span to its log lines, from a log line to its trace, and from a metric's sample
  to the trace it came from.

## Alternatives considered

- OpenTelemetry's C++ SDK. It builds its own protobuf and curl, which our build would compile beside
  the protobuf it has, and its exporter writes JSON by way of protobuf anyway. We have nlohmann's
  json and cpp-httplib already, and a span is small enough to write and show: the tracer is about 550
  lines, comments and all. Speaking OpenTelemetry's formats, Alloy and Tempo read it as they would the
  SDK's, and swapping it for the SDK later changes how spans are made, not where they go.
- Jaeger or Zipkin to keep the traces. Tempo keeps them the way Loki keeps logs, cheaply, by their
  ids, sits in the Grafana we have, beside Loki and Pyroscope, makes the service graph and the
  span metrics itself, and searches with TraceQL.
- Tracing the tick. It isn't a request, and nobody waits on one alone, while a span for each, 20 a
  second in each world, would be 1.7 million a day for what Tracy and the tick's histogram already
  show.
- Tracing the client. Its spans would come from players' computers, with clocks that differ from
  the servers', through a collector open to the internet that anyone could send spans to. The
  servers' spans start where their work does.
- The client carrying the traceparent, in its hello. A client could send any trace it liked. The
  token's record in Redis is ours, and goes from the login server to the world already.
- The session carrying the trace, from logging in to every request after it. A session lasts half
  an hour, through every request a player makes with it, which would make a trace with no end, so
  logging in is a trace of its own, and entering a world another.
- Sampling at the head, as a trace starts, which the SDK can do. It decides before anything's
  happened, so it keeps the slow and failed traces no more often than the rest, and they're the
  ones worth reading.

## Consequences

- A trace shows where a request's time went, and in which process, which neither metrics nor logs
  could: the first ones found the prepares, and from 0.87.0 a connection keeps the reads it's
  prepared ([Monitoring](../monitoring.md#what-the-traces-showed)).
- Every call to Redis and MySQL made for a traced request costs a span, a few hundred bytes and a
  lock to queue it. A call the tick makes costs a check of the thread's current span.
- Alloy holds each trace in memory for ten seconds, and Tempo's metrics count only what's kept: a
  tenth of the quick traces.
- A log line's trace opens only if Alloy kept it.
- The login server serves no metrics, so the spans it drops aren't counted.
- The hello still runs on the world's thread, and a handover's hellos still come all at once, so a
  handover still holds up the next instance's tick for as long as its players' hellos take.
