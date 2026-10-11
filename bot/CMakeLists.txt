add_library(nanommo_bot_core bot.cc wander.cc)
target_include_directories(nanommo_bot_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(nanommo_bot_core PUBLIC nanommo_protocol asio)

add_executable(nanommo_bot main.cc)
target_link_libraries(nanommo_bot PRIVATE nanommo_bot_core)

add_executable(nanommo_bot_tests wander_test.cc)
target_link_libraries(nanommo_bot_tests PRIVATE nanommo_bot_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(nanommo_bot_tests)
