FROM ubuntu:24.04 AS toolchain
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build ca-certificates doxygen pkg-config \
      xorg-dev libgl1-mesa-dev xvfb xauth libgl1-mesa-dri \
      libmysqlclient-dev libsodium-dev libssl-dev openssl \
 && rm -rf /var/lib/apt/lists/*

FROM toolchain AS build
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build

FROM build AS test
# xvfb-run gives the client's tests a virtual display to open their window on.
RUN xvfb-run --auto-servernum ctest --test-dir build --output-on-failure

# On Linux, Clang links GCC's runtime library unless told otherwise, and the
# sanitizers need some of Clang's own.
FROM toolchain AS clang
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      clang-18 libclang-rt-18-dev llvm-18 \
 && rm -rf /var/lib/apt/lists/*
ENV PATH=/usr/lib/llvm-18/bin:$PATH CC=clang CXX=clang++ \
    LDFLAGS="-rtlib=compiler-rt -unwindlib=libgcc"

FROM clang AS sanitize
ARG SANITIZE=address,undefined
ENV TSAN_OPTIONS=suppressions=/src/tsan.supp
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DNANOMMO_SANITIZE=${SANITIZE} \
 && cmake --build build \
 && xvfb-run --auto-servernum ctest --test-dir build --output-on-failure

FROM clang AS fuzz
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DNANOMMO_SANITIZE=address,undefined -DNANOMMO_FUZZ=ON \
 && cmake --build build --target stories_fuzzer world_fuzzer

FROM clang AS tidy
RUN apt-get update \
 && apt-get install -y --no-install-recommends clang-tidy-18 \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
# clang-tidy reads the headers protoc writes, so those come first.
RUN cmake -S . -B build -G Ninja -DNANOMMO_FUZZ=ON \
 && cmake --build build --target nanommo_protocol nanommo_content nanommo_client_core \
 && run-clang-tidy -quiet -p build \
      '^/src/(server|bot|client|protocol|content|terrain|fuzz)/(?!.*_test\.cc)'

FROM clang AS coverage
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DNANOMMO_COVERAGE=ON \
 && cmake --build build

FROM ubuntu:24.04 AS runtime
RUN apt-get update \
 && apt-get install -y --no-install-recommends libmysqlclient21 libsodium23 libssl3t64 \
 && rm -rf /var/lib/apt/lists/*

FROM runtime AS server
COPY --from=test /src/build/bin/nanommo_server /usr/local/bin/
COPY content/*.txtpb /usr/local/share/nanommo/content/
ENV NANOMMO_CONTENT=/usr/local/share/nanommo/content
USER ubuntu
EXPOSE 7777 7777/udp 9464
ENTRYPOINT ["nanommo_server"]

FROM runtime AS login
COPY --from=test /src/build/bin/nanommo_login /usr/local/bin/
USER ubuntu
EXPOSE 7778
ENTRYPOINT ["nanommo_login"]

FROM runtime AS saver
COPY --from=test /src/build/bin/nanommo_saver /usr/local/bin/
USER ubuntu
ENTRYPOINT ["nanommo_saver"]

FROM ubuntu:24.04 AS bot
RUN apt-get update \
 && apt-get install -y --no-install-recommends libssl3t64 \
 && rm -rf /var/lib/apt/lists/*
COPY --from=test /src/build/bin/nanommo_bot /usr/local/bin/
COPY content/*.txtpb /usr/local/share/nanommo/content/
ENV NANOMMO_CONTENT=/usr/local/share/nanommo/content
USER ubuntu
ENTRYPOINT ["nanommo_bot"]

FROM ubuntu:24.04 AS certificates
RUN apt-get update \
 && apt-get install -y --no-install-recommends openssl \
 && rm -rf /var/lib/apt/lists/*
COPY tls/certificates.sh /usr/local/bin/
ENTRYPOINT ["certificates.sh"]

FROM ubuntu:24.04 AS netem
RUN apt-get update \
 && apt-get install -y --no-install-recommends iproute2 ethtool \
 && rm -rf /var/lib/apt/lists/*
COPY bench/netem.sh /usr/local/bin/
ENTRYPOINT ["netem.sh"]

FROM build AS profile-build
RUN cmake -S . -B build -DTRACY_ENABLE=ON \
 && cmake --build build --target nanommo_server

# Ubuntu's perf wants a build for the running kernel, and Docker Desktop's
# kernel isn't Ubuntu's, so we put the generic build on the path.
FROM runtime AS profile
RUN apt-get update \
 && apt-get install -y --no-install-recommends linux-tools-generic perl \
 && rm -rf /var/lib/apt/lists/* \
 && ln -s /usr/lib/linux-tools/*/perf /usr/local/bin/perf
ADD --checksum=sha256:b1ba3aaaf296ab72ced4dca84514377a4b2c8122831d743f726c106d3d339c2a \
    https://github.com/brendangregg/FlameGraph/archive/41fee1f99f9276008b7cd112fca19dc3ea84ac32.tar.gz /tmp/
RUN mkdir /opt/flamegraph \
 && tar -xzf /tmp/41fee1f99f9276008b7cd112fca19dc3ea84ac32.tar.gz -C /opt/flamegraph --strip-components 1 \
 && rm /tmp/*.tar.gz
ENV PATH=/opt/flamegraph:$PATH
COPY --from=profile-build /src/build/bin/nanommo_server /usr/local/bin/
COPY content/*.txtpb /usr/local/share/nanommo/content/
ENV NANOMMO_CONTENT=/usr/local/share/nanommo/content
COPY bench/profile.sh /usr/local/bin/
EXPOSE 7777 7777/udp 9464 8086
ENTRYPOINT ["nanommo_server"]

FROM build AS docs-build
RUN cmake --build build --target docs

FROM scratch AS docs
COPY --from=docs-build /src/build/docs/html /

# FBX2glTF only comes built for x86-64 Linux, so on an Arm Mac it's emulated.
FROM --platform=linux/amd64 node:24.21.0-bookworm-slim AS characters-build
RUN apt-get update \
 && apt-get install -y --no-install-recommends unzip \
 && rm -rf /var/lib/apt/lists/*
ADD --checksum=sha256:52243ec1bf25c4501bdd4977057b6782702fe3ae1b79e2ac13039d2207870bec \
    https://github.com/godotengine/FBX2glTF/releases/download/v0.13.1/FBX2glTF-linux-x86_64.zip /tmp/
RUN unzip -p /tmp/FBX2glTF-linux-x86_64.zip FBX2glTF-linux-x86_64/FBX2glTF-linux-x86_64 \
      > /usr/local/bin/fbx2gltf \
 && chmod +x /usr/local/bin/fbx2gltf
WORKDIR /assets
COPY assets/tools/package.json assets/tools/package-lock.json tools/
RUN npm ci --prefix tools
COPY assets/tools/ tools/
COPY assets/mixamo/ mixamo/
RUN tools/build.sh

FROM scratch AS characters
COPY --from=characters-build /assets/out/ /

FROM python:3.13-slim AS lint
RUN pip install --no-cache-dir clang-format==18.1.3 cpplint==2.0.2
# cpplint names each header guard after the path from the folder above this
# one, so the folder is named after the project: NANOMMO_SERVER_TICK_H_.
WORKDIR /nanommo
COPY . .
RUN find server bot client protocol content terrain fuzz -name '*.cc' -o -name '*.h' \
 | xargs clang-format --dry-run --Werror \
 && cpplint --root=.. --recursive --quiet server bot client protocol content terrain fuzz
