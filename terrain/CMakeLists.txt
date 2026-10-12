add_library(nanommo_terrain noise.cc terrain.cc)
target_include_directories(nanommo_terrain PUBLIC ${PROJECT_SOURCE_DIR})
# Compilers for Arm, and for x86 with -march, fuse a multiply and an add into
# one instruction, which rounds once instead of twice. The heights would come
# out a little different on each machine.
target_compile_options(nanommo_terrain PRIVATE -ffp-contract=off)

add_executable(nanommo_terrain_tests terrain_test.cc)
target_link_libraries(nanommo_terrain_tests PRIVATE nanommo_terrain GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(nanommo_terrain_tests)
