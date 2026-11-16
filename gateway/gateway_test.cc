#include "gateway/gateway.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "asio/buffer.hpp"
#include "asio/connect.hpp"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/read.hpp"
#include "asio/read_until.hpp"
#include "asio/ssl/stream_base.hpp"
#include "asio/write.hpp"
#include "gateway/websocket.h"
#include "gateway/world_list.h"
#include "gtest/gtest.h"
#include "protocol/tls.h"

namespace nanommo {
namespace {

using asio::ip::tcp;

constexpr char kTls[] = NANOMMO_TEST_TLS "/ours/";

// A world at localhost:7777 to its own address; nothing anywhere else.
absl::StatusOr<std::string> OneWorld(std::string_view host, uint16_t port) {
  if (host == "localhost" && port == 7777) {
    return std::string("10.0.0.5");
  }
  return absl::NotFoundError("no world there");
}

TEST(GatewayRoutes, LeadsToTheLoginServerAndToWorldsTheLookupFinds) {
  const Route route =
      Routes({.host = "login", .port = 7778, .name = "login"}, OneWorld);
  const absl::StatusOr<Upstream> login = route("/login");
  ASSERT_TRUE(login.ok());
  EXPECT_EQ(login->host, "login");
  EXPECT_EQ(login->port, 7778);

  // A world is reached at its own address, and checked by its public name.
  const absl::StatusOr<Upstream> world = route("/world/localhost/7777");
  ASSERT_TRUE(world.ok()) << world.status();
  EXPECT_EQ(world->host, "10.0.0.5");
  EXPECT_EQ(world->port, 7777);
  EXPECT_EQ(world->name, "localhost");

  // The same host on another port, a port that isn't one, and anywhere else.
  EXPECT_EQ(route("/world/localhost/22").status().code(),
            absl::StatusCode::kNotFound);
  EXPECT_EQ(route("/world/localhost/77777").status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(route("/admin").status().code(), absl::StatusCode::kNotFound);
}

TEST(WorldList, FindsAListedWorldAndRefusesAStaleOrEmptyList) {
  asio::io_context io;
  WorldList worlds(io, [] { return std::vector<ListedWorld>{}; });
  const WorldList::Clock::time_point start = WorldList::Clock::now();

  // Before Consul has answered once, nothing is let through.
  EXPECT_EQ(worlds.Find("localhost", 7777, start).status().code(),
            absl::StatusCode::kUnavailable);

  worlds.Take(
      std::vector<ListedWorld>{
          {.public_host = "localhost", .port = 7777, .address = "10.0.0.5"}},
      start);
  const absl::StatusOr<std::string> found =
      worlds.Find("localhost", 7777, start + std::chrono::seconds(1));
  ASSERT_TRUE(found.ok()) << found.status();
  EXPECT_EQ(*found, "10.0.0.5");
  // The same host on a port the list doesn't have.
  EXPECT_EQ(worlds.Find("localhost", 7778, start).status().code(),
            absl::StatusCode::kNotFound);

  // Consul failing keeps the list, until it's too old to trust.
  worlds.Take(absl::UnavailableError("Consul is down"),
              start + std::chrono::seconds(5));
  EXPECT_TRUE(
      worlds.Find("localhost", 7777, start + std::chrono::seconds(9)).ok());
  EXPECT_EQ(worlds.Find("localhost", 7777, start + std::chrono::seconds(11))
                .status()
                .code(),
            absl::StatusCode::kUnavailable);

  // An empty list, fresh, has no worlds in it.
  worlds.Take(std::vector<ListedWorld>{}, start + std::chrono::seconds(12));
  EXPECT_EQ(worlds.Find("localhost", 7777, start + std::chrono::seconds(12))
                .status()
                .code(),
            absl::StatusCode::kNotFound);
}

TEST(WorldList, AsksOnAThreadOfItsOwnAndTakesTheAnswerOnTheIoThread) {
  asio::io_context io;
  WorldList worlds(io, [] {
    return std::vector<ListedWorld>{
        {.public_host = "localhost", .port = 7777, .address = "10.0.0.5"}};
  });
  worlds.Start();
  // The answer comes back on this thread, while the io_context runs.
  for (int i = 0;
       i < 100 && !worlds.Find("localhost", 7777, WorldList::Clock::now()).ok();
       ++i) {
    io.run_for(std::chrono::milliseconds(10));
  }
  EXPECT_TRUE(worlds.Find("localhost", 7777, WorldList::Clock::now()).ok());
  io.stop();
}

// A TLS server that sends back what it's sent, as a world would answer.
class Echo {
 public:
  Echo() : acceptor_(io_, tcp::endpoint(asio::ip::address_v4::loopback(), 0)) {
    thread_ = std::thread([this] { Serve(); });
  }
  ~Echo() {
    std::error_code ignored;
    acceptor_.close(ignored);
    thread_.join();
  }
  uint16_t port() const { return acceptor_.local_endpoint().port(); }

 private:
  void Serve() {
    absl::StatusOr<asio::ssl::context> tls = ServerTls(
        absl::StrCat(kTls, "server.crt"), absl::StrCat(kTls, "server.key"));
    std::error_code error;
    TlsStream stream(io_, *tls);
    acceptor_.accept(stream.lowest_layer(), error);
    if (!error) {
      stream.handshake(asio::ssl::stream_base::server, error);
    }
    std::array<char, 1024> buffer{};
    while (!error) {
      const std::size_t size = stream.read_some(asio::buffer(buffer), error);
      if (!error) {
        asio::write(stream, asio::buffer(buffer.data(), size), error);
      }
    }
  }

  asio::io_context io_;
  tcp::acceptor acceptor_;
  std::thread thread_;
};

class GatewayTest : public ::testing::Test {
 protected:
  void SetUp() override {
    absl::StatusOr<asio::ssl::context> tls =
        ClientTls(absl::StrCat(kTls, "ca.crt"));
    ASSERT_TRUE(tls.ok()) << tls.status();
    tls_ = std::make_unique<asio::ssl::context>(*std::move(tls));
    echo_ = std::make_unique<Echo>();
    const uint16_t echo = echo_->port();
    absl::StatusOr<std::unique_ptr<Gateway>> gateway = Gateway::Create(
        io_, 0, *tls_,
        [echo](std::string_view path) -> absl::StatusOr<Upstream> {
          if (path == "/echo") {
            return Upstream{
                .host = "127.0.0.1", .port = echo, .name = "localhost"};
          }
          return absl::NotFoundError("nothing there");
        });
    ASSERT_TRUE(gateway.ok()) << gateway.status();
    gateway_ = *std::move(gateway);
    thread_ = std::thread([this] { io_.run(); });
  }

  void TearDown() override {
    // The echo ends once the gateway, still running, closes its side of the
    // page's connection, which the test closed.
    echo_.reset();
    io_.stop();
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  // Opens the path as a page would, and returns the gateway's answer to the
  // handshake, up to its blank line.
  std::string Open(tcp::socket& socket, const std::string& path) {
    socket.connect({asio::ip::address_v4::loopback(), gateway_->port()});
    asio::write(socket,
                asio::buffer(absl::StrCat(
                    "GET ", path,
                    " HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
                    "Connection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n")));
    std::string answer;
    std::error_code error;
    const std::size_t size = asio::read_until(
        socket, asio::dynamic_buffer(answer), "\r\n\r\n", error);
    return answer.substr(0, size);
  }

  asio::io_context io_;
  std::unique_ptr<asio::ssl::context> tls_;
  std::unique_ptr<Echo> echo_;
  std::unique_ptr<Gateway> gateway_;
  std::thread thread_;
};

TEST_F(GatewayTest, CarriesAPagesMessagesToTheServerAndItsAnswerBack) {
  asio::io_context io;
  tcp::socket socket(io);
  const std::string answer = Open(socket, "/echo");
  EXPECT_TRUE(answer.starts_with("HTTP/1.1 101")) << answer;
  EXPECT_NE(answer.find("s3pPLMBiTxaQ9kYGzzhZRbK+xOo="), std::string::npos);

  asio::write(socket, asio::buffer(ClientFrame(Opcode::kBinary, "hello", 7)));
  // The echo comes back as the server's frame: binary, unmasked, 5 bytes.
  std::array<char, 7> frame{};
  asio::read(socket, asio::buffer(frame));
  EXPECT_EQ(std::string(frame.data(), frame.size()),
            std::string("\x82\x05hello", 7));
}

TEST_F(GatewayTest, TurnsDownAPathThatLeadsNowhere) {
  asio::io_context io;
  tcp::socket socket(io);
  const std::string answer = Open(socket, "/elsewhere");
  EXPECT_TRUE(answer.starts_with("HTTP/1.1 404")) << answer;
}

}  // namespace
}  // namespace nanommo
