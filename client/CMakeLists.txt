add_library(nanommo_client_core
  window.cc gui.cc menu.cc renderer.cc offscreen.cc
  camera.cc scene.cc input.cc connection.cc
  login_screen.cc nameplates.cc player_frame.cc
  animator.cc characters.cc
)
target_include_directories(nanommo_client_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(nanommo_client_core PUBLIC
  raylib imgui nanommo_protocol asio
  absl::memory absl::status absl::statusor absl::strings
)

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
)

add_executable(nanommo_client main.cc)
target_link_libraries(nanommo_client PRIVATE nanommo_client_core)
add_dependencies(nanommo_client nanommo_client_assets)

add_executable(nanommo_client_tests
  renderer_test.cc animator_test.cc characters_test.cc
)
target_link_libraries(nanommo_client_tests PRIVATE nanommo_client_core GTest::gtest_main)
add_dependencies(nanommo_client_tests nanommo_client_assets)

include(GoogleTest)
gtest_discover_tests(nanommo_client_tests)
