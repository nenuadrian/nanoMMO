add_executable(nanommo_client main.cc window.cc)
target_include_directories(nanommo_client PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(nanommo_client PRIVATE
  raylib absl::memory absl::status absl::statusor
)
