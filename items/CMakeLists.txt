add_library(nanommo_items items.proto items.cc)
protobuf_generate(TARGET nanommo_items
  IMPORT_DIRS ${PROJECT_SOURCE_DIR} PROTOC_OUT_DIR ${PROJECT_BINARY_DIR}
)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/items.pb.cc
  PROPERTIES COMPILE_OPTIONS -w
)
target_include_directories(nanommo_items PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
target_link_libraries(nanommo_items PUBLIC
  protobuf::libprotobuf absl::status absl::statusor absl::strings
)

add_executable(nanommo_items_tests items_test.cc)
target_link_libraries(nanommo_items_tests PRIVATE nanommo_items GTest::gtest_main)
target_compile_definitions(nanommo_items_tests PRIVATE
  NANOMMO_ITEMS="${CMAKE_CURRENT_SOURCE_DIR}/items.txtpb"
)

include(GoogleTest)
gtest_discover_tests(nanommo_items_tests)
