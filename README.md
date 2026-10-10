# nanoMMO

The game built in the YouTube series *Building an MMO: The Engineering Series*: a C++ server, a
C++ client, and everything around them. Each episode's code is a git tag (`ep001/end`,
`ep002/end`, ...).

## Build, test and run

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
./build/bin/nanommo_server
```

Or with only Docker installed:

```bash
docker build --target test .                  # build and run the tests on Ubuntu
docker build --target server -t nanommo-server .  # the server image, if the tests pass
docker run --rm nanommo-server                    # run the server
docker build --target docs --output site .    # API docs in site/
```
