# The gateway: WebSocket for browsers' pages, TLS to the servers.
add_library(nanommo_gateway_core websocket.cc)
target_include_directories(nanommo_gateway_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(nanommo_gateway_core PUBLIC
  OpenSSL::Crypto absl::status absl::statusor absl::strings
)

add_executable(nanommo_gateway_tests websocket_test.cc)
target_link_libraries(nanommo_gateway_tests PRIVATE nanommo_gateway_core GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(nanommo_gateway_tests)
