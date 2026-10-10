cmake_minimum_required(VERSION 3.28)
project(nanommo VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)
set(ABSL_PROPAGATE_CXX_STD ON)
FetchContent_Declare(
  abseil
  URL https://github.com/abseil/abseil-cpp/archive/refs/tags/20250512.1.tar.gz
  SYSTEM
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(abseil)

add_compile_options(-Wall -Wextra -Wpedantic)

add_subdirectory(server)
