# NPCs

Each land has a few NPCs near its middle, where players start. In the Meadows, a guard and a
storyteller stand on either side of the road to the gate, and a merchant and a healer face each
other across it. The Ashlands have an ash warden and a storyteller, a merchant and a healer. Each
land has a trainer too, a few steps from where players start. The storytellers tell stories (see
[The storyteller](#the-storyteller)), the trainers teach skills (see
[Learning skills](gameplay.md#learning-skills)), the guard, the ash warden and the healers give quests (see
[Quests](gameplay.md#quests)), and the merchants don't do anything yet. Each one is a person, like the
players, with its name over it in green.

Kinds of NPC, and where each NPC stands, are in `content/npcs.txtpb` (see [Content](content.md)).
The server leaves out an NPC whose ground isn't walkable, which another world's seed could do.

An NPC isn't a monster that stands still. It has no health, level or experience, and nothing to
think with, so it's a small `Npc` of its own in each land's `World`, with ids of its own, apart
from the monsters'. NPCs are never saved, as the content puts them back where they were whenever a
server starts.

- NPCs never move, so the server puts them into their interest cells once, when it spawns them, and
  each player gets the NPCs near them, like the players and monsters (see
  [What each player gets](networking.md#what-each-player-gets)).
- They never change either, so they go out once a second, with the names and levels, in every
  twentieth world, by kind.
- The client keeps each NPC for three seconds after the world it last came in was sent
  (`kNpcTimeout`, in `protocol/parts.h`), so one lost datagram, or two, doesn't make them blink out
  over UDP. It drops one that's been gone for longer, such as when you walk away from it, and a new
  land starts with none.
- The client draws every NPC as a person, on the players' characters, in what its kind wears, with
  its own skin and hair (see [Characters](graphics.md#characters)), and picks one with a box around where a
  person stands. Until episode forty-five, each was a ball of its kind's colour and size.

By kind, the Meadows' four NPCs take 74 bytes in each world they go in, half of what their names and
looks took.

An NPC that walks a round, or whose look changes, would have to go out with every world, like a
monster, or in a message of its own whenever it changes.

## The storyteller

The storyteller is an NPC that tells you a story about a few words you give it. Click it, in purple
across the road from the guard, or from the ash warden, type a few words, such as
`dragon castle gold`, and press Enter, or Tell me a story. The story comes back in the chat, under
the storyteller's name, to you and nobody else.

The stories come from [mvGPT](https://github.com/nenuadrian/mvGPT), a small language model built
from scratch in a series of its own, which learned to write short children's stories from a few
words, and is on the Hugging Face Hub at its `ep003-sft` tag. `storyteller/` is mvGPT's completions
server, from its `ep014/end` tag: an OpenAI-style `POST /v1/completions` on port 8000, which writes
the requests that wait at the same time together, up to 16 at once (see `storyteller/README.md`).
Compose runs it as the `storyteller` service, and keeps the model it downloads the first time in a
volume:

```bash
docker compose up -d --build storyteller
curl localhost:8000/v1/completions -H 'Content-Type: application/json' \
  -d '{"prompt": "Words: dragon, castle, gold\nStory:", "max_tokens": 100}'
```

- A click on the world makes a ray from the camera through the mouse (raylib's
  `GetScreenToWorldRay`), and the client picks the nearest NPC that talks along it, by a box around
  where a person stands (`GetRayCollisionBox`, in `client/picking.h`), if you're within talking
  reach of it (`kTalkReach`, four metres, in `content/npcs.h`, which the server holds you to too).
  The cursor is a hand over one you can click. Each NPC says whether it talks, from its kind.
- The words go to the server with your input, as `talk`, with the NPC's id. The server checks that
  the NPC is in your land and talks, and that you're within four metres of it, and cleans the words
  like a line of chat (see [Chat](gameplay.md#chat)).
- The prompt is the one mvGPT learned from, with the first three words, in lowercase:
  `Words: dragon, castle, gold\nStory:` (`server/stories.h`). Its vocabulary is the 10,000 commonest
  words in its stories, so it knows dragon, castle and sword, and a word it doesn't know, such as
  warden or healer, reaches it as unknown.
- Each player can ask for a story every ten seconds, from a token bucket of their own. The server
  tells a player who's too far away, or asks too soon, in a line with no name.
- The server asks the storyteller for a hundred tokens at most, from 16 threads of its own
  (`server/storyteller.h`), so that a tick never waits for the model, and the storyteller can write
  16 stories at once. A story takes a second or two. One that takes longer than ten seconds fails,
  and so does one when the storyteller is down, and the NPC says it can't think of a story.
- A story that ran out of tokens stops in the middle of a sentence, so the server ends it at its last
  full stop, and breaks it into lines of chat of at most 200 bytes, after the last sentence that
  fits. Each line is cleaned like a player's, and they go to you over TCP, like chat.
- The server counts the stories in `nanommo_stories_total`, told, refused or failed, and how long
  each took, from asking to telling, in `nanommo_story_duration_seconds`. The dashboard shows the
  stories a minute, and the median and 99th percentile wait.

The world server finds the storyteller at `NANOMMO_STORYTELLER_HOST` (`127.0.0.1` by default), which
Compose sets to `storyteller`. Any server with OpenAI's completions API can take its place, such as
vLLM, as long as its model writes stories from words like these.
