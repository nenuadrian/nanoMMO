add_library(nanommo_client_core window.cc gui.cc)
target_include_directories(nanommo_client_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(nanommo_client_core PUBLIC
  raylib imgui
  absl::memory absl::status absl::statusor
)

add_executable(nanommo_client main.cc)
target_link_libraries(nanommo_client PRIVATE nanommo_client_core)
