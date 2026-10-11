add_library(nanommo_server_core
  tick.cc listen.cc world.cc session.cc server.cc
)
target_include_directories(nanommo_server_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(nanommo_server_core PUBLIC
  nanommo_protocol asio
  absl::memory absl::status absl::statusor absl::strings
)

add_executable(nanommo_server main.cc)
target_link_libraries(nanommo_server PRIVATE nanommo_server_core absl::time)

add_executable(nanommo_server_tests tick_test.cc world_test.cc)
target_link_libraries(nanommo_server_tests PRIVATE nanommo_server_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(nanommo_server_tests)
