add_library(nanommo_cinema_core film.proto film.cc)
protobuf_generate(TARGET nanommo_cinema_core
  IMPORT_DIRS ${PROJECT_SOURCE_DIR} PROTOC_OUT_DIR ${PROJECT_BINARY_DIR}
)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/film.pb.cc
  PROPERTIES COMPILE_OPTIONS -w
)
target_include_directories(nanommo_cinema_core PUBLIC
  ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR}
)
target_link_libraries(nanommo_cinema_core PUBLIC
  raylib nanommo_protocol nanommo_content nanommo_terrain
  protobuf::libprotobuf absl::status absl::statusor absl::strings
)

add_executable(nanommo_cinema main.cc)
target_link_libraries(nanommo_cinema PRIVATE nanommo_cinema_core nanommo_client_core)
add_dependencies(nanommo_cinema nanommo_client_assets)
