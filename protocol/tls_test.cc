#include "protocol/tls.h"

#include <string>
#include <system_error>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "asio/error.hpp"
#include "asio/io_context.hpp"
#include "asio/ip/address.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/ssl/host_name_verification.hpp"
#include "asio/ssl/stream_base.hpp"
#include "gtest/gtest.h"

namespace {

// Two authorities the build made for this test: ours, and someone else's.
constexpr char kOurs[] = NANOMMO_TEST_TLS "/ours/";
constexpr char kTheirs[] = NANOMMO_TEST_TLS "/theirs/";

// A client that checks the certificate for host's name shakes hands with the
// server, over this machine's loopback.
bool Connects(asio::ssl::context& server_tls, asio::ssl::context& client_tls,
              const std::string& host) {
  asio::io_context io;
  asio::ip::tcp::acceptor acceptor(io,
                                   {asio::ip::make_address("127.0.0.1"), 0});
  nanommo::TlsStream server(io, server_tls);
  nanommo::TlsStream client(io, client_tls);
  client.set_verify_callback(asio::ssl::host_name_verification(host));

  acceptor.async_accept(server.lowest_layer(), [&](std::error_code error) {
    if (!error) {
      server.async_handshake(asio::ssl::stream_base::server,
                             [](std::error_code) {});
    }
  });
  std::error_code result = asio::error::not_connected;
  client.lowest_layer().async_connect(
      acceptor.local_endpoint(), [&](std::error_code error) {
        if (!error) {
          client.async_handshake(
              asio::ssl::stream_base::client,
              [&](std::error_code handshake) { result = handshake; });
        }
      });
  io.run();
  return !result;
}

}  // namespace

TEST(Tls, TrustsOurAuthorityOnly) {
  absl::StatusOr<asio::ssl::context> server = nanommo::ServerTls(
      absl::StrCat(kOurs, "server.crt"), absl::StrCat(kOurs, "server.key"));
  absl::StatusOr<asio::ssl::context> ours =
      nanommo::ClientTls(absl::StrCat(kOurs, "ca.crt"));
  absl::StatusOr<asio::ssl::context> theirs =
      nanommo::ClientTls(absl::StrCat(kTheirs, "ca.crt"));
  ASSERT_TRUE(server.ok() && ours.ok() && theirs.ok());

  EXPECT_TRUE(Connects(*server, *ours, "localhost"));
  EXPECT_FALSE(Connects(*server, *theirs, "localhost"));
  EXPECT_FALSE(Connects(*server, *ours, "nanommo.example.com"));
  EXPECT_FALSE(nanommo::ClientTls(absl::StrCat(kOurs, "missing.crt")).ok());
}
