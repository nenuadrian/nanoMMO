FROM ubuntu:24.04 AS toolchain
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build ca-certificates doxygen \
      xorg-dev libgl1-mesa-dev xvfb xauth libgl1-mesa-dri \
 && rm -rf /var/lib/apt/lists/*

FROM toolchain AS build
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build

FROM build AS test
# xvfb-run gives the client's tests a virtual display to open their window on.
RUN xvfb-run --auto-servernum ctest --test-dir build --output-on-failure

FROM ubuntu:24.04 AS server
COPY --from=test /src/build/bin/nanommo_server /usr/local/bin/
USER ubuntu
EXPOSE 7777 9464
ENTRYPOINT ["nanommo_server"]

FROM ubuntu:24.04 AS bot
COPY --from=test /src/build/bin/nanommo_bot /usr/local/bin/
USER ubuntu
ENTRYPOINT ["nanommo_bot"]

FROM build AS docs-build
RUN cmake --build build --target docs

FROM scratch AS docs
COPY --from=docs-build /src/build/docs/html /

FROM python:3.13-slim AS lint
RUN pip install --no-cache-dir clang-format==18.1.3 cpplint==2.0.2
# cpplint names each header guard after the path from the folder above this
# one, so the folder is named after the project: NANOMMO_SERVER_TICK_H_.
WORKDIR /nanommo
COPY . .
RUN find server bot client protocol -name '*.cc' -o -name '*.h' \
 | xargs clang-format --dry-run --Werror \
 && cpplint --root=.. --recursive --quiet server bot client protocol
