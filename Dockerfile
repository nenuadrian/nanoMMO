FROM ubuntu:24.04 AS toolchain
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build ca-certificates doxygen \
 && rm -rf /var/lib/apt/lists/*

FROM toolchain AS build
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build

FROM build AS test
RUN ctest --test-dir build --output-on-failure

FROM ubuntu:24.04 AS server
COPY --from=test /src/build/bin/nanommo_server /usr/local/bin/
USER ubuntu
ENTRYPOINT ["nanommo_server"]

FROM build AS docs-build
RUN cmake --build build --target docs

FROM scratch AS docs
COPY --from=docs-build /src/build/docs/html /
