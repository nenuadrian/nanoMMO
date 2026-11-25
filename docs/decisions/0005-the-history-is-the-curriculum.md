# 5. The repository's history is the curriculum

## Status

Accepted

## Context

The game is built in a video series, a step at a time, and from episode 1 each episode's code is
a git tag that a viewer can check out, build and run. So the history isn't a by-product of the
work. It's what the series teaches from, and a choice about history is a choice about what a
viewer finds at a tag.

## Decision

- `main` is linear, with one teachable commit per step, tagged `epNNN/step-M`, and the episode's
  last step `epNNN/end`. Each commit's message says what changed and why, and ends with its step,
  such as `Step: ep070/step-1`.
- Prototypes go on throwaway `lab/<arc>` branches, a few episodes ahead, where dead ends stay.
- Until an episode is published, its steps are rewritten freely. A bug found later is fixed in the
  step that made it, the steps after it are replayed, and their tags move, with the old refs kept
  as backups.
- Once an episode is published, its tags never move. A change of mind about published code is a
  step in a later episode.
- Some steps fail on purpose, to show an error and how to fix it, such as a rounding bug in
  `ep001/step-2`, which step 3's test catches, or a migration Flyway refuses in `ep018/step-2`. The
  commit says so, so a check that builds every tag knows to expect it.
- The episodes ahead of the videos stay in a private copy of the repository, and the public one
  gets each episode's tags as it's released. From 0.59.0, a release is tagged with its version too
  ([Versions and dependencies](../releases.md)).

## Alternatives considered

- Fixing a bug in the episode where it's found, as most projects would. The steps before it would
  keep the bug, and a viewer who checks one out would meet it with nothing to explain it. On
  16 October 2026, `ep023/step-2` patched a key that `ep017/step-3` got wrong, and the fix moved
  back to episode 17.

## Consequences

- Any step can be checked out and built, and `git log ep001/end..ep070/end` reads as the course.
- Rewriting unpublished steps means replaying the commits after them and moving their tags, so
  each rewrite keeps the refs from before it, as `backup/` branches.
- One exception so far. Episode 28's sanitizers, fuzzers and clang-tidy look at the code as it
  stood, so what they found in earlier steps was fixed in episode 28, and nothing moved back.
- The client's first history, on Vulkan, wasn't thrown away. The `vulkan-series` branch keeps it,
  and nothing builds on it ([0006](0006-draw-the-client-with-vulkan.md)).
- These records follow the same rule as a published step. Once accepted, a record doesn't change,
  and a later one supersedes it ([README](README.md)).
