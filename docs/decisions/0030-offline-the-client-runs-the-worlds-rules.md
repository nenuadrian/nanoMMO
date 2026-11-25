# 30. Offline, the client runs the world's rules itself

## Status

Accepted

## Context

The client in a browser ([0029](0029-the-client-in-a-browser-with-emscripten.md)) can't reach the
servers, as a page can't open a TCP or UDP socket, and the servers speak nothing else. So a page
showed the login screen and could go no further. Someone opening the game's website should be able
to play it, and someone with a build should be able to without the servers running.

The world's rules were in the world server, beside its database and network code, and what a
player's inputs do and what they're shown were in `server/server.cc`.

## Decision

- The world's rules are a library, `nanommo_world`, without the database or the network. It holds
  `World` and what it needs, `server/shown.h` for what each player is shown, and `server/inputs.h`
  for what their numbered inputs do and when. The world server links it, and so does the client.
- `LocalConnection` (`client/local_connection.h`) is a `Link`, as `Connection` is, that runs a
  world of the client's own with that library. That world is the first land, its monsters and
  NPCs, and one player, at the server's 50-millisecond tick. Its inputs go through `Inputs` and
  `DoItem`, and its worlds through `ToProtocol`, `YouIn`, `BagOf` and `QuestsOf`, the server's own,
  so nothing is copied.
- A browser plays offline from the start (`CanReachServers()` is false there), and a computer does
  with `nanommo_client --offline`. Whatever lives on the servers, the storyteller, guilds, trades
  and the other lands, says so in the chat. Nothing is saved.
- Online, nothing changes. The server decides, and the client's copy of the rules is only used
  offline.

## Alternatives considered

- The world server itself in the page, over a pretend socket. It brings the session, the network
  and the saving code, with MySQL, Redis and Consul behind them, none of which a page has.
- A server for the website's players, reached over WebSockets. It's a server to run and pay for,
  and a second transport to keep, for players who've not chosen to play yet; it may still come,
  for playing online in a browser.
- The client's own rules for offline play, simpler than the server's. They'd drift from the
  server's, and offline would teach a player a different game.

## Consequences

- A change to the world's rules plays offline as it does online, from the same code and tests
  (`server/shown_test.cc`, `server/inputs_test.cc`).
- The client carries the world's rules, which makes it, and the page, bigger.
- What the servers own (accounts, saving, guilds, other players) has no offline version, and a
  player who plays offline and then online starts again online.
