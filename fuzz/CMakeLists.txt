foreach(fuzzer stories world)
  add_executable(${fuzzer}_fuzzer ${fuzzer}.cc)
  target_link_libraries(${fuzzer}_fuzzer PRIVATE nanommo_server_core)
  target_link_options(${fuzzer}_fuzzer PRIVATE -fsanitize=fuzzer)
endforeach()
target_compile_definitions(world_fuzzer PRIVATE
  NANOMMO_CONTENT_FOLDER="${PROJECT_SOURCE_DIR}/content"
)
