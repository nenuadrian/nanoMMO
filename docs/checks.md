# Code style and checks

## Code style

The code follows [Google's C++ style guide](https://google.github.io/styleguide/cppguide.html).
clang-format lays it out, and cpplint checks the rest. `docker build --target lint .` runs both, as
CI does.

## Checking the code

The tests check what we thought to test. Four more checks look for what we didn't. In a public
repository, where GitHub's runners are free, CI runs each of them on each push. A private one pays
for its minutes, so a push there gets the tests, and clang-tidy, the sanitizers and the fuzzers run
once a night, or when started by hand (`gh workflow run CI`). The coverage report, which only a
public repository publishes, isn't made there at all. Either way, a check that passed on the same
tree doesn't run again, as after a rebase that moves commits but not code, and a newer push to a
branch cancels the checks still running for the one before.

Each stage of the Dockerfile that compiles does it through ccache, which keeps what the compiler
made in a cache mount that outlives the build, so protobuf and the other libraries compile once,
and a build after a change compiles what changed. Each build prints how much it found there
(`ccache --show-stats`). The mount lives in Docker's build cache, so it stays on your machine;
CI has a fresh one for each job, and moves the job's ccache in and out of GitHub's cache around
its builds.

The sanitizers build checks into the code, which catch it doing something wrong as it runs.
`-DNANOMMO_SANITIZE=address,undefined` builds everything, the libraries too, with AddressSanitizer,
for reads and writes out of bounds or after a free, and leaks, and UndefinedBehaviorSanitizer, for
signed overflow, bad shifts and casts and the like. Both stop the program at the first one.
`-DNANOMMO_SANITIZE=thread` builds it with ThreadSanitizer instead, for data races, which it
reports as it goes. The code runs about twice as slowly with the first two, and five to fifteen
times as slowly with ThreadSanitizer. The `sanitize` stage builds with Clang and runs the tests:

```bash
docker build --target sanitize .
docker build --target sanitize --build-arg SANITIZE=thread .
```

ThreadSanitizer keeps its own record of memory at addresses that clash with the kernel's when the
kernel randomizes them with more than 28 bits. Docker Desktop's kernel uses 18, and Ubuntu's 32 or
33, as on GitHub's runners or in a Colima VM. There, lower it first, once after the machine starts:

```bash
docker run --rm --privileged alpine sysctl -w vm.mmap_rnd_bits=28
```

ThreadSanitizer sees threads start, and each lock and atomic in the code it built. Under it, httplib
looks names up without the thread glibc starts for that, and `tsan.supp` leaves out what Mesa's
library does with its threads, as Mesa draws the client's tests in Docker and orders its threads in
code ThreadSanitizer can't follow.

`compose.sanitize.yaml` runs the login server, the world server, the saver, the bots and the tests
from the `sanitize` stage, so that the bots play against servers built with the sanitizers. What the
sanitizers find goes to their logs:

```bash
export COMPOSE_FILE=compose.yaml:compose.sanitize.yaml
docker compose up -d --build login server saver
docker compose up -d bots
docker compose logs server bots
```

`SANITIZE=thread` in front of the `up` builds them with ThreadSanitizer, and `docker compose run --rm
tests` runs the tests against MySQL, Redis and Consul with the sanitizers on.

The fuzzers in `fuzz/` run with libFuzzer, which comes with Clang. Each is a function that takes
bytes, and libFuzzer calls it with input after input, made from the inputs that reached code the
others didn't, with the sanitizers on. `stories_fuzzer` hands the storyteller's lines whatever a
model might write, and checks that each line fits in the chat, whole characters and all.
`world_fuzzer` plays the game with whatever a client might send, and checks that each player stays
somewhere real, with health between nothing and their most, that their bag holds no stack
bigger than it can, in a slot that's there, and wears each item in its slot, and that each quest
they're on is one the content has, with no more kills than it asks for, and not one they've done.

```bash
docker build --target fuzz -t nanommo-fuzz .
docker run --rm nanommo-fuzz build/bin/stories_fuzzer -max_total_time=60
docker run --rm nanommo-fuzz build/bin/world_fuzzer -max_total_time=60
```

When a fuzzer breaks a check, it stops and prints the input that did it, and saves it to a file
named `crash-` and its hash, which the fuzzer runs again when given that file.

clang-tidy reads the code for bugs that compile: `.clang-tidy` turns on its checks for likely bugs,
the static analyzer, and its checks for threads and for performance, less a few that flag what we
mean to do, and makes each finding an error. `docker build --target tidy .` runs it on the code,
without the tests.

Last, the coverage report shows which lines the tests run, and how often. `docker compose run --rm
coverage` builds the code with Clang's counters, runs all of the tests against MySQL, Redis and
Consul, and writes the report to `site/coverage`. It reads the servers, the bot and the client too,
so that code no test reaches counts, at zero. CI publishes it with the API docs, at
[nenuadrian.github.io/nanoMMO/coverage](https://nenuadrian.github.io/nanoMMO/coverage/), from a
public repository only, as Pages would put a private copy's code where anyone can read it.

`patches/` holds fixes to the libraries we fetch that their releases don't have yet, which CMake
applies as it fetches them: civetweb's for a data race on the flag that stops its threads, which
ThreadSanitizer found.
