# The gateway: WebSocket for browsers' pages, TLS to the servers. Consul's
# client is in the server's library.
add_library(nanommo_gateway_core websocket.cc gateway.cc world_list.cc)
target_include_directories(nanommo_gateway_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(nanommo_gateway_core PUBLIC
  nanommo_protocol asio OpenSSL::Crypto
  absl::status absl::statusor absl::strings absl::str_format
)

add_executable(nanommo_gateway main.cc)
target_link_libraries(nanommo_gateway PRIVATE nanommo_gateway_core nanommo_server_core)

add_executable(nanommo_gateway_tests websocket_test.cc gateway_test.cc)
target_link_libraries(nanommo_gateway_tests PRIVATE nanommo_gateway_core GTest::gtest_main)
# The gateway's test talks TLS with the certificates the protocol tests made.
add_dependencies(nanommo_gateway_tests nanommo_test_certificates)
target_compile_definitions(nanommo_gateway_tests PRIVATE
  NANOMMO_TEST_TLS="${PROJECT_BINARY_DIR}/protocol/tls"
)
include(GoogleTest)
gtest_discover_tests(nanommo_gateway_tests)
