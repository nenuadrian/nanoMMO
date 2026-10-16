add_library(nanommo_content
  items.proto monsters.proto npcs.proto
  text.cc items.cc monsters.cc npcs.cc content.cc
)
protobuf_generate(TARGET nanommo_content
  IMPORT_DIRS ${PROJECT_SOURCE_DIR} PROTOC_OUT_DIR ${PROJECT_BINARY_DIR}
)
set_source_files_properties(
  ${CMAKE_CURRENT_BINARY_DIR}/items.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/monsters.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/npcs.pb.cc
  PROPERTIES COMPILE_OPTIONS -w
)
target_include_directories(nanommo_content PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
target_link_libraries(nanommo_content PUBLIC
  nanommo_terrain protobuf::libprotobuf absl::status absl::statusor absl::strings
)

add_executable(nanommo_content_tests content_test.cc)
target_link_libraries(nanommo_content_tests PRIVATE nanommo_content GTest::gtest_main)
target_compile_definitions(nanommo_content_tests PRIVATE
  NANOMMO_CONTENT="${CMAKE_CURRENT_SOURCE_DIR}"
)

include(GoogleTest)
gtest_discover_tests(nanommo_content_tests)
