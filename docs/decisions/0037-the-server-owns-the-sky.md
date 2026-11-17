# 37. The server owns the sky

## Status

Accepted

## Context

Each land had one look, all the time: its sky, its fog, its sun in one place, and as many of its
clouds out as its `cloudiness` said. A player could change it for themselves on the settings' Sky
page, with colour pickers and four looks to pick from. Nobody else saw what they picked, and
nothing in the world changed on its own.

We wanted a day that turns into night, and weather: rain and storms over the Meadows, ash and fire
falling in the Ashlands, mist. Two players standing side by side should see the same rain, and the
same sunset.

## Decision

- The sky is a pure function of the land and the wall clock: `SkyAt(land, unix_ms)` in
  `server/weather.h`, in the world's library. A day is 40 minutes. The weather comes in spells, at
  most one in each 15-minute slot, from 6 minutes to the whole slot, which come in and go out over
  90 seconds. Which weather, how long and how strong come from the slot and the land's seed, by
  each land's own odds: rain, storms, cloud and morning mist in the Meadows, ash and fire in the
  Ashlands, mist in the Crypt.
- The world server works it out each tick, once for each land it runs, and sends it in each
  player's own part of the world (`World.sky`): the time of day, the weather, how strong it is, and
  the wind. Offline, `LocalConnection` sends the same, from the same function.
- The client draws what it's sent (`client/daylight.h`, `client/weather.h`), and doesn't check
  it. The settings have two switches, Weather and Day and night, and the Sky page
  is gone.

## Alternatives considered

- Each client works the sky out for itself from the same function and its own clock. The clocks
  of players' computers differ, by minutes on some, so two players side by side would see the rain
  start at different times. The server's clock is the one everybody shares already.
- The server keeps the weather as state, changing it at random as it goes, and saves it. It's
  state to keep in step between the instances of a land, and to lose on a restart, for something
  a function of the time gives for nothing.
- Weather as an event, sent when it changes. A player who joins in the middle of a storm needs it
  sent too, and a lost datagram would leave someone in the sun. The sky is about 25 bytes, so it
  goes in every world.

## Consequences

- The sky is only for looking: the server doesn't check whether a client draws it, and a client
  that ignores it, or that a player turns off, gains nothing. The rules of the world don't read
  it.
- A future event, a storm over a boss fight, say, is a change to `SkyAt` or a value the server
  sends in its place, without a change to the client.
- Every land's weather can be found for any moment, past or future, from the function alone, which
  the tests do (`server/weather_test.cc`).
