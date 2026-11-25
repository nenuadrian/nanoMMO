add_library(nanommo_protocol
  nanommo.proto chat.cc framing.cc messages.cc parts.cc places.cc settings.cc version.cc
)
# Logging in, over TLS, takes sockets, which a browser doesn't give us.
if(NOT EMSCRIPTEN)
  target_sources(nanommo_protocol PRIVATE login_client.cc tls.cc)
  target_link_libraries(nanommo_protocol PUBLIC OpenSSL::SSL absl::time)
endif()
protobuf_generate(TARGET nanommo_protocol
  IMPORT_DIRS ${PROJECT_SOURCE_DIR} PROTOC_OUT_DIR ${PROJECT_BINARY_DIR}
)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/nanommo.pb.cc
  PROPERTIES COMPILE_OPTIONS -w
)
# The version, from project(), as a header next to the generated protocol.
configure_file(version.h.in version.h @ONLY)
target_include_directories(nanommo_protocol PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
# Where the repository is, for the files the programs read from it.
set_source_files_properties(settings.cc PROPERTIES
  COMPILE_DEFINITIONS NANOMMO_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(nanommo_protocol PUBLIC
  nanommo_content protobuf::libprotobuf absl::status absl::statusor absl::strings
  asio
)

# The tests run on this machine, not in a browser.
if(EMSCRIPTEN)
  return()
endif()

add_executable(nanommo_protocol_tests
  protocol_test.cc framing_test.cc parts_test.cc tls_test.cc chat_test.cc login_client_test.cc
  settings_test.cc version_test.cc messages_test.cc
)
target_link_libraries(nanommo_protocol_tests PRIVATE nanommo_protocol GTest::gtest_main)

# The TLS test's certificates, from two authorities: ours and someone else's.
set(test_tls ${CMAKE_CURRENT_BINARY_DIR}/tls)
foreach(authority ours theirs)
  add_custom_command(
    OUTPUT ${test_tls}/${authority}/server.crt
    COMMAND ${PROJECT_SOURCE_DIR}/scripts/certificates.sh ${test_tls}/${authority}
    DEPENDS ${PROJECT_SOURCE_DIR}/scripts/certificates.sh
  )
endforeach()
add_custom_target(nanommo_test_certificates
  DEPENDS ${test_tls}/ours/server.crt ${test_tls}/theirs/server.crt
)
add_dependencies(nanommo_protocol_tests nanommo_test_certificates)
target_compile_definitions(nanommo_protocol_tests PRIVATE NANOMMO_TEST_TLS="${test_tls}")

include(GoogleTest)
gtest_discover_tests(nanommo_protocol_tests)
