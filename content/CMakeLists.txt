add_library(nanommo_content
  figures.proto items.proto monsters.proto npcs.proto quests.proto rig.proto
  skills.proto recipes.proto factions.proto
  text.cc figures.cc items.cc monsters.cc npcs.cc quests.cc rig.cc skills.cc
  recipes.cc factions.cc content.cc
)
protobuf_generate(TARGET nanommo_content
  IMPORT_DIRS ${PROJECT_SOURCE_DIR} PROTOC_OUT_DIR ${PROJECT_BINARY_DIR}
)
set_source_files_properties(
  ${CMAKE_CURRENT_BINARY_DIR}/factions.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/figures.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/items.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/monsters.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/npcs.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/quests.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/recipes.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/rig.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/skills.pb.cc
  PROPERTIES COMPILE_OPTIONS -w
)
target_include_directories(nanommo_content PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
target_link_libraries(nanommo_content PUBLIC
  nanommo_terrain protobuf::libprotobuf absl::status absl::statusor absl::strings
)

add_executable(nanommo_content_tests content_test.cc)
target_link_libraries(nanommo_content_tests PRIVATE nanommo_content GTest::gtest_main)
target_compile_definitions(nanommo_content_tests PRIVATE
  NANOMMO_CONTENT="${CMAKE_CURRENT_SOURCE_DIR}"
)

include(GoogleTest)
gtest_discover_tests(nanommo_content_tests)

# The content as a graph in smollerGraph, for checks that follow it from a new
# player to each thing in the game, and a tool that writes it out for sg to
# serve. The game's own programs don't link it, and a browser's build has no
# smollerGraph (the top CMakeLists.txt fetches it for the servers and tools).
# Nor does Windows: smollerGraph's pages are read and written with POSIX's
# pread and pwrite.
if(NOT EMSCRIPTEN AND NOT WIN32)
add_library(nanommo_content_graph graph.cc)
target_link_libraries(nanommo_content_graph PUBLIC
  nanommo_content smollergraph::smollergraph absl::status absl::statusor
  absl::strings
)

add_executable(nanommo_content_graph_tests graph_test.cc)
target_link_libraries(nanommo_content_graph_tests PRIVATE
  nanommo_content_graph GTest::gtest_main
)
target_compile_definitions(nanommo_content_graph_tests PRIVATE
  NANOMMO_CONTENT="${CMAKE_CURRENT_SOURCE_DIR}"
)
gtest_discover_tests(nanommo_content_graph_tests)

add_executable(nanommo_graph graph_main.cc)
target_link_libraries(nanommo_graph PRIVATE nanommo_content_graph)
endif()
