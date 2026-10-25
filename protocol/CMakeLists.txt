add_library(nanommo_protocol
  nanommo.proto chat.cc framing.cc login_client.cc parts.cc places.cc tls.cc
)
protobuf_generate(TARGET nanommo_protocol
  IMPORT_DIRS ${PROJECT_SOURCE_DIR} PROTOC_OUT_DIR ${PROJECT_BINARY_DIR}
)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/nanommo.pb.cc
  PROPERTIES COMPILE_OPTIONS -w
)
target_include_directories(nanommo_protocol PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
target_link_libraries(nanommo_protocol PUBLIC
  nanommo_content protobuf::libprotobuf absl::status absl::statusor absl::strings
  asio OpenSSL::SSL
)

add_executable(nanommo_protocol_tests
  protocol_test.cc framing_test.cc parts_test.cc tls_test.cc chat_test.cc
)
target_link_libraries(nanommo_protocol_tests PRIVATE nanommo_protocol GTest::gtest_main)

# The TLS test's certificates, from two authorities: ours and someone else's.
set(test_tls ${CMAKE_CURRENT_BINARY_DIR}/tls)
foreach(authority ours theirs)
  add_custom_command(
    OUTPUT ${test_tls}/${authority}/server.crt
    COMMAND ${PROJECT_SOURCE_DIR}/tls/certificates.sh ${test_tls}/${authority}
    DEPENDS ${PROJECT_SOURCE_DIR}/tls/certificates.sh
  )
endforeach()
add_custom_target(nanommo_test_certificates
  DEPENDS ${test_tls}/ours/server.crt ${test_tls}/theirs/server.crt
)
add_dependencies(nanommo_protocol_tests nanommo_test_certificates)
target_compile_definitions(nanommo_protocol_tests PRIVATE NANOMMO_TEST_TLS="${test_tls}")

include(GoogleTest)
gtest_discover_tests(nanommo_protocol_tests)
