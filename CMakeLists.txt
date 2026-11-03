cmake_minimum_required(VERSION 3.28)
project(nanommo VERSION 0.63.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# With Clang, CMake looks for C++20 modules, which we don't use.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_compile_options(-fno-omit-frame-pointer)

# address,undefined, or thread, which can't go with address.
set(NANOMMO_SANITIZE "" CACHE STRING "Sanitizers to build with")
if(NANOMMO_SANITIZE)
  add_compile_options(-fsanitize=${NANOMMO_SANITIZE} -fno-sanitize-recover=all)
  add_link_options(-fsanitize=${NANOMMO_SANITIZE})
endif()

include(FetchContent)
set(ABSL_PROPAGATE_CXX_STD ON)
FetchContent_Declare(
  abseil
  URL https://github.com/abseil/abseil-cpp/archive/refs/tags/20260817.0.tar.gz
  URL_HASH SHA256=f7e05179df39c45434cad433f5783840bb3788ef322976f9138bc6b72b3a107d
  SYSTEM
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(abseil)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
  URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
)
FetchContent_MakeAvailable(googletest)
enable_testing()

FetchContent_Declare(
  raylib
  URL https://github.com/raysan5/raylib/archive/refs/tags/6.2.tar.gz
  URL_HASH SHA256=df7a13129b1bec9e73a6962efff170beba310ee6eab8f94cd000b7316b4db5ee
  SYSTEM
)
FetchContent_Declare(
  imgui
  URL https://github.com/ocornut/imgui/archive/refs/tags/v1.93.0.tar.gz
  URL_HASH SHA256=f7bf6bc76e5d79d40d751096663e6c5d18daf08129dd52e7f7b09e4251c8eeee
)
FetchContent_Declare(
  rlimgui
  URL https://github.com/raylib-extras/rlImGui/archive/1550009359ad975927f7f0e4a3f47e3f27123ea9.tar.gz
  URL_HASH SHA256=7d04ff0c996a89190e1050d3c4448ad784479723c76893efea2d731bd5eb839f
)
FetchContent_MakeAvailable(raylib imgui rlimgui)
# By default, raylib skins on the CPU. This leaves it to our vertex shader.
target_compile_definitions(raylib PRIVATE SUPPORT_GPU_SKINNING=1)

set(protobuf_INSTALL OFF)
set(protobuf_WITH_ZLIB OFF)
FetchContent_Declare(
  protobuf
  URL https://github.com/protocolbuffers/protobuf/archive/refs/tags/v36.2.tar.gz
  URL_HASH SHA256=7e7eaf580ec77f1a392ff1ab1013c13d3b04a6716f002f697f67f3c1113f0530
  SYSTEM
)
FetchContent_MakeAvailable(protobuf)
include(${protobuf_SOURCE_DIR}/cmake/protobuf-generate.cmake)

FetchContent_Declare(
  asio
  URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-38-2.tar.gz
  URL_HASH SHA256=9f2648fa483e58a6bf848d970ee0ea650ca19ed7769dfa520ed4f7b8d27af1db
)
FetchContent_MakeAvailable(asio)
add_library(asio INTERFACE)
target_include_directories(asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/asio/include)

# Tracy's macros compile to nothing without -DTRACY_ENABLE=ON. On demand,
# it records nothing until a profiler connects.
set(TRACY_ON_DEMAND ON)
FetchContent_Declare(
  tracy
  URL https://github.com/wolfpld/tracy/archive/refs/tags/v0.14.1.tar.gz
  URL_HASH SHA256=bf4af567e9c7524d07f3caa745fad02fb33bd5694f11910750382d1efbb251c1
  SYSTEM
)
FetchContent_MakeAvailable(tracy)

set(ENABLE_PUSH OFF)
set(ENABLE_COMPRESSION OFF)
set(ENABLE_TESTING OFF)
FetchContent_Declare(
  prometheus-cpp
  URL https://github.com/jupp0r/prometheus-cpp/releases/download/v1.3.0/prometheus-cpp-with-submodules.tar.gz
  URL_HASH SHA256=62bc2cc9772db2314dbaae506ae2a75c8ee897dab053d8729e86a637b018fdb6
  PATCH_COMMAND patch -p1 -i ${PROJECT_SOURCE_DIR}/patches/civetweb-stop-flag.patch
  SYSTEM
)
FetchContent_MakeAvailable(prometheus-cpp)
# civetweb serves the metrics, and stops its threads with a plain int unless
# we ask for atomics.
target_compile_definitions(civetweb PRIVATE STOP_FLAG_NEEDS_LOCK)

set(BUILD_SHARED_LIBS OFF)
set(DISABLE_TESTS ON)
FetchContent_Declare(
  hiredis
  URL https://github.com/redis/hiredis/archive/refs/tags/v1.5.0.tar.gz
  URL_HASH SHA256=eec7ef8641d221e144a58d5ef8d733c8c75c7fd487097a233aacb4fa33981cfb
  SYSTEM
)
FetchContent_MakeAvailable(hiredis)

FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  SYSTEM
)
FetchContent_MakeAvailable(json)

# Consul speaks plain HTTP inside Docker's network, so httplib needs no TLS or
# compression.
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF)
set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF)
set(HTTPLIB_NO_EXCEPTIONS ON)
set(HTTPLIB_INSTALL OFF)
# glibc looks names up for httplib on a thread ThreadSanitizer can't see.
if(NANOMMO_SANITIZE MATCHES "thread")
  set(HTTPLIB_USE_NON_BLOCKING_GETADDRINFO OFF)
endif()
FetchContent_Declare(
  httplib
  URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.64.0.tar.gz
  URL_HASH SHA256=1ada413782af5e67e80452c39b7c90beb6fa4fc8341e18c0896bf95684b9a620
  SYSTEM
)
FetchContent_MakeAvailable(httplib)

find_package(PkgConfig REQUIRED)
# Homebrew keeps mysql-client and OpenSSL out of the places pkg-config and
# CMake look in.
if(APPLE)
  foreach(package mysql-client openssl@3)
    execute_process(
      COMMAND brew --prefix ${package}
      OUTPUT_VARIABLE prefix
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    list(APPEND CMAKE_PREFIX_PATH ${prefix})
  endforeach()
endif()
pkg_check_modules(mysqlclient REQUIRED IMPORTED_TARGET mysqlclient)
pkg_check_modules(sodium REQUIRED IMPORTED_TARGET libsodium)
find_package(OpenSSL 3 REQUIRED)

add_library(imgui
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${rlimgui_SOURCE_DIR}/rlImGui.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${rlimgui_SOURCE_DIR})
target_link_libraries(imgui PUBLIC raylib)
target_compile_options(imgui PRIVATE -w)

add_compile_options(-Wall -Wextra -Wpedantic)

option(NANOMMO_COVERAGE "Count the lines the tests run, with Clang" OFF)
if(NANOMMO_COVERAGE)
  add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
  add_link_options(-fprofile-instr-generate)
endif()

option(NANOMMO_FUZZ "Build the fuzzers in fuzz/, with Clang" OFF)
if(NANOMMO_FUZZ)
  # So that libFuzzer sees which of our code each input reaches.
  add_compile_options(-fsanitize=fuzzer-no-link)
endif()

add_subdirectory(protocol)
add_subdirectory(content)
add_subdirectory(terrain)
add_subdirectory(server)
add_subdirectory(bot)
add_subdirectory(client)
add_subdirectory(cinema)
if(NANOMMO_FUZZ)
  add_subdirectory(fuzz)
endif()

find_package(Doxygen)
if(DOXYGEN_FOUND)
  FetchContent_Declare(
    doxygen_awesome
    URL https://github.com/jothepro/doxygen-awesome-css/archive/refs/tags/v2.5.0.tar.gz
    URL_HASH SHA256=959b6e9369a0f5c7c0a9beba34b18a02bc5e788e68f6733975908f133a784dd5
  )
  FetchContent_MakeAvailable(doxygen_awesome)

  set(DOXYGEN_PROJECT_NAME "nanoMMO")
  set(DOXYGEN_PROJECT_BRIEF "Building an MMO: The Engineering Series")
  set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/docs)
  set(DOXYGEN_USE_MDFILE_AS_MAINPAGE README.md)
  set(DOXYGEN_EXTRACT_ALL YES)
  set(DOXYGEN_EXCLUDE_PATTERNS *_test.cc)
  set(DOXYGEN_GENERATE_TREEVIEW YES)
  set(DOXYGEN_DISABLE_INDEX NO)
  set(DOXYGEN_FULL_SIDEBAR NO)
  set(DOXYGEN_HTML_COLORSTYLE LIGHT)
  set(DOXYGEN_HTML_EXTRA_STYLESHEET
    ${doxygen_awesome_SOURCE_DIR}/doxygen-awesome.css
    ${doxygen_awesome_SOURCE_DIR}/doxygen-awesome-sidebar-only.css
  )
  # A link to a section of another page, such as docs/gameplay.md#items, works
  # on GitHub, but Doxygen 1.9 leaves it as it is, which leads nowhere, so in
  # the API docs it goes to the page.
  set(DOXYGEN_FILTER_PATTERNS "*.md=sed -E 's/[.]md#[a-z0-9-]+[)]/.md)/g'")
  # Each page in docs/ is a page of its own in the menu, after the README, in
  # the order the README lists them.
  doxygen_add_docs(docs
    README.md
    docs/gameplay.md docs/content.md docs/npcs.md docs/graphics.md docs/cinema.md
    docs/island.md docs/worlds.md docs/networking.md docs/accounts.md
    docs/saving.md docs/website.md docs/monitoring.md docs/benchmarking.md
    docs/deployment.md docs/checks.md docs/releases.md
    protocol content terrain server bot client)
endif()
