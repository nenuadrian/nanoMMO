foreach(fuzzer stories world)
  add_executable(${fuzzer}_fuzzer ${fuzzer}.cc)
  target_link_libraries(${fuzzer}_fuzzer PRIVATE nanommo_server_core)
  target_link_options(${fuzzer}_fuzzer PRIVATE -fsanitize=fuzzer)
endforeach()
target_link_libraries(stories_fuzzer PRIVATE utf8_validity)
target_include_directories(stories_fuzzer PRIVATE ${protobuf_SOURCE_DIR}/third_party)
target_compile_definitions(world_fuzzer PRIVATE
  NANOMMO_CONTENT_FOLDER="${PROJECT_SOURCE_DIR}/content"
)
