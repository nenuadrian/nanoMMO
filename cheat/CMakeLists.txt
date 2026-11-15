add_library(nanommo_cheat_core measure.cc)
target_include_directories(nanommo_cheat_core PUBLIC ${PROJECT_SOURCE_DIR})

add_executable(nanommo_cheat main.cc)
target_link_libraries(nanommo_cheat PRIVATE nanommo_cheat_core nanommo_client_core)

add_executable(nanommo_cheat_tests measure_test.cc)
target_link_libraries(nanommo_cheat_tests PRIVATE nanommo_cheat_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(nanommo_cheat_tests)
