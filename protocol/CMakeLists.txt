add_library(nanommo_protocol
  nanommo.proto framing.cc login_client.cc parts.cc
)
protobuf_generate(TARGET nanommo_protocol
  IMPORT_DIRS ${PROJECT_SOURCE_DIR} PROTOC_OUT_DIR ${PROJECT_BINARY_DIR}
)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/nanommo.pb.cc
  PROPERTIES COMPILE_OPTIONS -w
)
target_include_directories(nanommo_protocol PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
target_link_libraries(nanommo_protocol PUBLIC protobuf::libprotobuf absl::status absl::statusor absl::strings asio)

add_executable(nanommo_protocol_tests
  protocol_test.cc framing_test.cc parts_test.cc
)
target_link_libraries(nanommo_protocol_tests PRIVATE nanommo_protocol GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(nanommo_protocol_tests)
