# The bag's rules, which the client runs too, to show a move before the
# server's done it.
add_library(nanommo_inventory inventory.cc)
target_include_directories(nanommo_inventory PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(nanommo_inventory PUBLIC
  nanommo_protocol nanommo_content absl::status absl::strings
)

# How a player walks, which the client runs too, to walk you before the
# server's answer comes. Like the terrain, it doesn't fuse a multiply and an
# add, so a step comes out the same on every machine.
add_library(nanommo_walking walking.cc)
target_include_directories(nanommo_walking PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(nanommo_walking PUBLIC nanommo_terrain)
target_compile_options(nanommo_walking PRIVATE -ffp-contract=off)

# The world's rules: its players, monsters and NPCs, their levels and skills,
# their trades and duels, where its players have been, and who's near whom.
# They need no database or network, so the client can run them too.
add_library(nanommo_world
  world.cc trades.cc duels.cc history.cc monsters.cc npcs.cc levels.cc interest.cc inputs.cc shown.cc
)
target_include_directories(nanommo_world PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(nanommo_world PUBLIC
  nanommo_inventory nanommo_walking nanommo_protocol nanommo_content nanommo_terrain
  absl::status absl::strings
)

# The rest are the servers, which a browser doesn't run. Tracy's zones compile
# to nothing without TRACY_ENABLE, so there, the world takes its header alone.
if(EMSCRIPTEN)
  target_include_directories(nanommo_world SYSTEM PUBLIC ${tracy_SOURCE_DIR}/public)
  return()
endif()
target_link_libraries(nanommo_world PUBLIC Tracy::TracyClient)

add_library(nanommo_server_core
  tick.cc listen.cc copies.cc session.cc
  server.cc metrics.cc password.cc database.cc events.cc
  accounts.cc characters.cc guilds.cc players.cc redis.cc tokens.cc
  login_server.cc consul.cc token_bucket.cc stories.cc storyteller.cc recorder.cc
)
target_include_directories(nanommo_server_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(nanommo_server_core PUBLIC
  nanommo_world nanommo_inventory nanommo_walking nanommo_protocol nanommo_content nanommo_terrain asio prometheus-cpp::core Tracy::TracyClient
  PkgConfig::sodium PkgConfig::mysqlclient hiredis
  httplib::httplib nlohmann_json::nlohmann_json
  absl::memory absl::status absl::statusor absl::strings absl::time absl::absl_check
)

add_executable(nanommo_server main.cc)
target_link_libraries(nanommo_server PRIVATE nanommo_server_core absl::time prometheus-cpp::pull)

add_executable(nanommo_login login_main.cc)
target_link_libraries(nanommo_login PRIVATE nanommo_server_core)

add_executable(nanommo_saver saver_main.cc)
target_link_libraries(nanommo_saver PRIVATE nanommo_server_core absl::time prometheus-cpp::pull)

add_executable(nanommo_recorder recorder_main.cc)
target_link_libraries(nanommo_recorder PRIVATE nanommo_server_core)

add_executable(nanommo_server_tests
  tick_test.cc inventory_test.cc walking_test.cc world_test.cc trades_test.cc duels_test.cc history_test.cc copies_test.cc monsters_test.cc interest_test.cc
  levels_test.cc inputs_test.cc shown_test.cc metrics_test.cc password_test.cc
  database_test.cc accounts_test.cc guilds_test.cc players_test.cc redis_test.cc tokens_test.cc
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
