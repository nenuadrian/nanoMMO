cmake_minimum_required(VERSION 3.28)
project(nanommo VERSION 0.1.0 LANGUAGES CXX)

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
  URL https://github.com/abseil/abseil-cpp/archive/refs/tags/20250512.1.tar.gz
  SYSTEM
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(abseil)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
)
FetchContent_MakeAvailable(googletest)
enable_testing()

FetchContent_Declare(
  raylib
  URL https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz
  SYSTEM
)
FetchContent_Declare(
  imgui
  URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b.tar.gz
)
FetchContent_Declare(
  rlimgui
  URL https://github.com/raylib-extras/rlImGui/archive/1550009359ad975927f7f0e4a3f47e3f27123ea9.tar.gz
)
FetchContent_MakeAvailable(raylib imgui rlimgui)
# By default, raylib skins on the CPU. This leaves it to our vertex shader.
target_compile_definitions(raylib PRIVATE SUPPORT_GPU_SKINNING=1)

set(protobuf_INSTALL OFF)
set(protobuf_WITH_ZLIB OFF)
FetchContent_Declare(
  protobuf
  URL https://github.com/protocolbuffers/protobuf/archive/refs/tags/v36.2.tar.gz
  SYSTEM
)
FetchContent_MakeAvailable(protobuf)
include(${protobuf_SOURCE_DIR}/cmake/protobuf-generate.cmake)

FetchContent_Declare(
  asio
  URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-38-2.tar.gz
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
  SYSTEM
)
FetchContent_MakeAvailable(tracy)

set(ENABLE_PUSH OFF)
set(ENABLE_COMPRESSION OFF)
set(ENABLE_TESTING OFF)
FetchContent_Declare(
  prometheus-cpp
  URL https://github.com/jupp0r/prometheus-cpp/releases/download/v1.3.0/prometheus-cpp-with-submodules.tar.gz
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
  URL https://github.com/redis/hiredis/archive/refs/tags/v1.4.1.tar.gz
  SYSTEM
)
FetchContent_MakeAvailable(hiredis)

FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
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
  URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.57.1.tar.gz
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
if(NANOMMO_FUZZ)
  add_subdirectory(fuzz)
endif()

find_package(Doxygen)
if(DOXYGEN_FOUND)
  FetchContent_Declare(
    doxygen_awesome
    URL https://github.com/jothepro/doxygen-awesome-css/archive/refs/tags/v2.5.0.tar.gz
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
  doxygen_add_docs(docs README.md protocol content terrain server bot client)
endif()
