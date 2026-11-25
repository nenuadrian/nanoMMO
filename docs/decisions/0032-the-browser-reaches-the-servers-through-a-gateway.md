# 32. A browser reaches the servers through a WebSocket gateway

## Status

Accepted

## Context

The client in a browser ([0029](0029-the-client-in-a-browser-with-emscripten.md)) plays offline
([0030](0030-offline-the-client-runs-the-worlds-rules.md)), as a page can't open a TCP or UDP
socket and the servers speak nothing else. A page can open a WebSocket, to the host it names. The
servers speak TLS over TCP to clients on computers (decision 14, with our own authority), and send
the worlds over UDP where they can (decision 12).

## Decision

- A gateway, `nanommo_gateway`, takes pages' WebSockets and connects each to a server over TLS, as
  a client on a computer does, and carries the game's messages both ways as they are, with the
  same framing and the same messages, so neither the servers nor the protocol change.
- Its two routes are `/login` to the login server, and `/world/<host>/<port>` to a world only where
  Consul lists one there, connected at its own address and checked by its public name. It isn't an
  open proxy.
- In a browser, a world comes over the same WebSocket, a reliable stream, as there's no UDP.
- The client's message handling (`Inbox`) is shared by `Connection` and `WebSocketLink`; a page
  opened with `?gateway=` logs in, and one opened without plays offline.
- The page reaches the gateway with `ws://` on this machine. Over the internet, a reverse proxy with
  a public certificate puts `wss://` in front of it, as a browser trusts no other.

## Alternatives considered

- WebSocket in each server, beside TLS. Each server would carry a second way in, and its TLS
  certificate would have to be one browsers trust.
- websockify, a ready-made WebSocket-to-TCP bridge. It can open TLS to the servers, but it would
  connect pages to anything it's told to, unless each world's address were in its configuration.
- WebTransport, for unreliable datagrams like UDP's. It needs HTTP/3 and a public certificate on
  the server, and isn't in every browser yet.

## Consequences

- A page plays against the real servers, with other players, saving and the storyteller.
- The gateway is one more service to run and watch, and every page's traffic goes through it.
- In a browser, the world comes over a stream, so a lost packet holds up what comes after it until
  it's sent again, where UDP would have gone on.
