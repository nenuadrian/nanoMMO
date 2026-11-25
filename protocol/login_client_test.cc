#include "protocol/login_client.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <system_error>
#include <thread>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "asio/buffer.hpp"
#include "asio/io_context.hpp"
#include "asio/ip/address.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/ssl/stream_base.hpp"
#include "asio/write.hpp"
#include "gtest/gtest.h"
#include "protocol/framing.h"
#include "protocol/tls.h"

namespace {

using std::chrono::milliseconds;
using std::chrono::steady_clock;

constexpr char kOurs[] = NANOMMO_TEST_TLS "/ours/";

nanommo::protocol::LoginRequest Request() {
  nanommo::protocol::LoginRequest request;
  request.set_username("alice");
  request.set_password("correct horse");
  return request;
}

TEST(LogIn, GivesUpOnALoginServerThatDoesntAnswer) {
  // The kernel takes the connection, as it does for a login server that's
  // hung, and nothing ever answers.
  asio::io_context io;
  const asio::ip::tcp::acceptor silent(
      io, {asio::ip::make_address("127.0.0.1"), 0});
  absl::StatusOr<asio::ssl::context> tls =
      nanommo::ClientTls(absl::StrCat(kOurs, "ca.crt"));
  ASSERT_TRUE(tls.ok()) << tls.status();

  const steady_clock::time_point start = steady_clock::now();
  const absl::StatusOr<nanommo::protocol::LoginReply> reply =
      nanommo::LogIn("127.0.0.1", silent.local_endpoint().port(), *tls,
                     Request(), milliseconds(200));
  const steady_clock::duration took = steady_clock::now() - start;
  EXPECT_TRUE(absl::IsDeadlineExceeded(reply.status())) << reply.status();
  EXPECT_TRUE(
      absl::StrContains(reply.status().message(), "didn't answer within 200ms"))
      << reply.status();
  EXPECT_GE(took, milliseconds(200));
  EXPECT_LT(took, milliseconds(1000));
}

TEST(LogIn, GetsTheAnswerOfOneThatAnswersInTime) {
  absl::StatusOr<asio::ssl::context> server_tls = nanommo::ServerTls(
      absl::StrCat(kOurs, "server.crt"), absl::StrCat(kOurs, "server.key"));
  absl::StatusOr<asio::ssl::context> tls =
      nanommo::ClientTls(absl::StrCat(kOurs, "ca.crt"));
  ASSERT_TRUE(server_tls.ok() && tls.ok());
  // A login server that answers each request with a session, after 50
  // milliseconds.
  asio::io_context io;
  asio::ip::tcp::acceptor acceptor(io,
                                   {asio::ip::make_address("127.0.0.1"), 0});
  nanommo::TlsStream server(io, *server_tls);
  nanommo::FrameReader reader;
  std::array<char, 1024> buffer;
  nanommo::protocol::LoginReply answer;
  answer.set_ok(true);
  answer.set_session("a session");
  const std::string frame = nanommo::Frame(answer);
  std::thread login([&] {
    std::error_code error;
    acceptor.accept(server.lowest_layer(), error);
    server.handshake(asio::ssl::stream_base::server, error);
    while (!error && !reader.Next()) {
      const std::size_t size = server.read_some(asio::buffer(buffer), error);
      reader.Append({buffer.data(), size});
    }
    std::this_thread::sleep_for(milliseconds(50));
    asio::write(server, asio::buffer(frame), error);
  });

  // The address the login server listens on, not localhost, which Windows
  // tries as ::1 first, and gives up on only after two seconds.
  const absl::StatusOr<nanommo::protocol::LoginReply> reply =
      nanommo::LogIn("127.0.0.1", acceptor.local_endpoint().port(), *tls,
                     Request(), milliseconds(2000));
  login.join();
  ASSERT_TRUE(reply.ok()) << reply.status();
  EXPECT_EQ(reply->session(), "a session");
}

}  // namespace
