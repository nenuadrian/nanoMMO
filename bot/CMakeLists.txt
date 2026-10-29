add_library(nanommo_bot_core bag.cc bot.cc bot_metrics.cc quests.cc wander.cc)
target_include_directories(nanommo_bot_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(nanommo_bot_core PUBLIC
  nanommo_protocol nanommo_content nanommo_terrain asio prometheus-cpp::core
)

add_executable(nanommo_bot main.cc)
target_link_libraries(nanommo_bot PRIVATE nanommo_bot_core prometheus-cpp::pull)

add_executable(nanommo_bot_tests bag_test.cc quests_test.cc wander_test.cc)
target_link_libraries(nanommo_bot_tests PRIVATE nanommo_bot_core GTest::gtest_main)
target_compile_definitions(nanommo_bot_tests PRIVATE
  NANOMMO_CONTENT="${PROJECT_SOURCE_DIR}/content"
)

include(GoogleTest)
gtest_discover_tests(nanommo_bot_tests)
