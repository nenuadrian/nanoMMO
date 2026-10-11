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

set(protobuf_INSTALL OFF)
set(protobuf_WITH_ZLIB OFF)
FetchContent_Declare(
  protobuf
  URL https://github.com/protocolbuffers/protobuf/archive/refs/tags/v36.2.tar.gz
  SYSTEM
)
FetchContent_MakeAvailable(protobuf)
include(${protobuf_SOURCE_DIR}/cmake/protobuf-generate.cmake)

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

add_subdirectory(protocol)
add_subdirectory(server)
add_subdirectory(client)

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
  doxygen_add_docs(docs README.md protocol server client)
endif()
