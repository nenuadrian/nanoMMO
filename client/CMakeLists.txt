add_library(nanommo_client_core
  settings.proto
  window.cc audio.cc jukebox.cc sound.cc settings.cc gui.cc menu.cc renderer.cc
  offscreen.cc
  camera.cc scene.cc input.cc connection.cc
  login_screen.cc nameplates.cc player_frame.cc hotbar.cc character_screen.cc
  chat_window.cc talk_window.cc picking.cc
  animator.cc characters.cc triangles.cc ground.cc trees.cc chunks.cc
  gate.cc
)
protobuf_generate(TARGET nanommo_client_core
  IMPORT_DIRS ${PROJECT_SOURCE_DIR} PROTOC_OUT_DIR ${PROJECT_BINARY_DIR}
)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/settings.pb.cc
  PROPERTIES COMPILE_OPTIONS -w
)
target_include_directories(nanommo_client_core PUBLIC
  ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR}
)
target_link_libraries(nanommo_client_core PUBLIC
  raylib imgui nanommo_protocol nanommo_content nanommo_terrain asio
  absl::memory absl::status absl::statusor absl::strings
)

file(GLOB content_files CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/content/*.txtpb)
add_custom_target(nanommo_client_assets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    ${PROJECT_SOURCE_DIR}/assets/characters
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/characters
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    ${PROJECT_SOURCE_DIR}/assets/monsters
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/monsters
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    ${PROJECT_SOURCE_DIR}/assets/weapons
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/weapons
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    ${PROJECT_SOURCE_DIR}/assets/trees
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/trees
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    ${PROJECT_SOURCE_DIR}/assets/music
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/music
  COMMAND ${CMAKE_COMMAND} -E make_directory
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/content
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${content_files} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/content
)

add_executable(nanommo_client main.cc)
target_link_libraries(nanommo_client PRIVATE nanommo_client_core)
add_dependencies(nanommo_client nanommo_client_assets)

add_executable(nanommo_client_bench bench_main.cc)
target_link_libraries(nanommo_client_bench PRIVATE nanommo_client_core)
add_dependencies(nanommo_client_bench nanommo_client_assets)

add_executable(nanommo_client_tests
  renderer_test.cc animator_test.cc characters_test.cc picking_test.cc
  jukebox_test.cc sound_test.cc settings_test.cc
)
target_link_libraries(nanommo_client_tests PRIVATE nanommo_client_core GTest::gtest_main)
add_dependencies(nanommo_client_tests nanommo_client_assets)

include(GoogleTest)
gtest_discover_tests(nanommo_client_tests)
