add_library(nanommo_server_core tick.cc)
target_include_directories(nanommo_server_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(nanommo_server_core PUBLIC absl::status absl::statusor)

add_executable(nanommo_server main.cc)
target_link_libraries(nanommo_server PRIVATE nanommo_server_core absl::time)
