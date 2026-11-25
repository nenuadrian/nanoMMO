#include "server/listen.h"

#include <array>
#include <cstdint>
#include <system_error>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/ip/udp.hpp"
#include "gtest/gtest.h"

namespace {

// A port someone else has for UDP, and that's free for TCP, the way Docker's
// DNS can have the one the system gives a server's TCP. The system picks a
// free TCP port, and the socket takes its number for UDP.
asio::ip::udp::socket TakenForUdp(asio::io_context& io) {
  for (;;) {
    asio::ip::tcp::acceptor tcp(io, {asio::ip::tcp::v4(), 0});
    const uint16_t port = tcp.local_endpoint().port();
    tcp.close();
    asio::ip::udp::socket udp(io, asio::ip::udp::v4());
    std::error_code error;
    udp.bind({asio::ip::udp::v4(), port}, error);
    if (!error) {
      return udp;
    }
  }
}

TEST(ListenAndBind, ListensAndBindsOnePortTheSystemPicks) {
  asio::io_context io;
  asio::ip::tcp::acceptor acceptor(io);
  asio::ip::udp::socket socket(io);
  const std::array<uint16_t, 1> ports = {0};
  ASSERT_TRUE(nanommo::ListenAndBind(acceptor, socket, ports).ok());
  EXPECT_NE(acceptor.local_endpoint().port(), 0);
  EXPECT_EQ(socket.local_endpoint().port(), acceptor.local_endpoint().port());
}

TEST(ListenAndBind, NamesAPortTakenForUdp) {
  asio::io_context io;
  const asio::ip::udp::socket taken = TakenForUdp(io);
  const uint16_t port = taken.local_endpoint().port();
  asio::ip::tcp::acceptor acceptor(io);
  asio::ip::udp::socket socket(io);
  const std::array<uint16_t, 1> ports = {port};
  const absl::Status status = nanommo::ListenAndBind(acceptor, socket, ports);
  EXPECT_EQ(status.code(), absl::StatusCode::kUnavailable);
  EXPECT_TRUE(absl::StrContains(
      status.message(), absl::StrCat("couldn't bind UDP port ", port, ":")))
      << status;
  // And it let go of the port's TCP.
  EXPECT_FALSE(acceptor.is_open());
  EXPECT_FALSE(socket.is_open());
}

// What a server on port 0 does when the system gives its TCP a port taken for
// UDP: it tries the next, and listens and binds there.
TEST(ListenAndBind, MovesOnFromAPortTakenForUdp) {
  asio::io_context io;
  const asio::ip::udp::socket taken = TakenForUdp(io);
  const uint16_t port = taken.local_endpoint().port();
  asio::ip::tcp::acceptor acceptor(io);
  asio::ip::udp::socket socket(io);
  const std::array<uint16_t, 2> ports = {port, 0};
  ASSERT_TRUE(nanommo::ListenAndBind(acceptor, socket, ports).ok());
  EXPECT_NE(acceptor.local_endpoint().port(), port);
  EXPECT_EQ(socket.local_endpoint().port(), acceptor.local_endpoint().port());
}

}  // namespace
