add_library(nanommo_server_core
  tick.cc listen.cc world.cc monsters.cc levels.cc session.cc interest.cc
  server.cc metrics.cc password.cc database.cc
  accounts.cc players.cc redis.cc tokens.cc
  login_server.cc
)
target_include_directories(nanommo_server_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(nanommo_server_core PUBLIC
  nanommo_protocol nanommo_terrain asio prometheus-cpp::core Tracy::TracyClient
  PkgConfig::sodium PkgConfig::mysqlclient hiredis
  absl::memory absl::status absl::statusor absl::strings absl::absl_check
)

add_executable(nanommo_server main.cc)
target_link_libraries(nanommo_server PRIVATE nanommo_server_core absl::time prometheus-cpp::pull)

add_executable(nanommo_login login_main.cc)
target_link_libraries(nanommo_login PRIVATE nanommo_server_core)

add_executable(nanommo_saver saver_main.cc)
target_link_libraries(nanommo_saver PRIVATE nanommo_server_core absl::time)

add_executable(nanommo_server_tests
  tick_test.cc world_test.cc monsters_test.cc interest_test.cc
  levels_test.cc metrics_test.cc password_test.cc
  accounts_test.cc players_test.cc tokens_test.cc
  login_server_test.cc
)
target_link_libraries(nanommo_server_tests PRIVATE nanommo_server_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(nanommo_server_tests)
