add_library(nanommo_server_core
  tick.cc listen.cc inventory.cc world.cc copies.cc monsters.cc npcs.cc levels.cc session.cc interest.cc
  server.cc metrics.cc password.cc database.cc events.cc
  accounts.cc players.cc redis.cc tokens.cc
  login_server.cc consul.cc token_bucket.cc stories.cc storyteller.cc recorder.cc
)
target_include_directories(nanommo_server_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(nanommo_server_core PUBLIC
  nanommo_protocol nanommo_content nanommo_terrain asio prometheus-cpp::core Tracy::TracyClient
  PkgConfig::sodium PkgConfig::mysqlclient hiredis
  httplib::httplib nlohmann_json::nlohmann_json
  absl::memory absl::status absl::statusor absl::strings absl::time absl::absl_check
)

add_executable(nanommo_server main.cc)
target_link_libraries(nanommo_server PRIVATE nanommo_server_core absl::time prometheus-cpp::pull)

add_executable(nanommo_login login_main.cc)
target_link_libraries(nanommo_login PRIVATE nanommo_server_core)

add_executable(nanommo_saver saver_main.cc)
target_link_libraries(nanommo_saver PRIVATE nanommo_server_core absl::time)

add_executable(nanommo_recorder recorder_main.cc)
target_link_libraries(nanommo_recorder PRIVATE nanommo_server_core)

add_executable(nanommo_server_tests
  tick_test.cc inventory_test.cc world_test.cc copies_test.cc monsters_test.cc interest_test.cc
  levels_test.cc metrics_test.cc password_test.cc
  database_test.cc accounts_test.cc players_test.cc tokens_test.cc
  login_server_test.cc consul_test.cc token_bucket_test.cc stories_test.cc
  events_test.cc recorder_test.cc
  server_test.cc
)
target_link_libraries(nanommo_server_tests PRIVATE nanommo_server_core GTest::gtest_main)
# The server's test talks TLS with the certificates the protocol tests made.
add_dependencies(nanommo_server_tests nanommo_test_certificates)
target_compile_definitions(nanommo_server_tests PRIVATE
  NANOMMO_TEST_TLS="${PROJECT_BINARY_DIR}/protocol/tls"
  NANOMMO_CONTENT="${PROJECT_SOURCE_DIR}/content"
)

include(GoogleTest)
gtest_discover_tests(nanommo_server_tests)
