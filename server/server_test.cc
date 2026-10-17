#include "server/server.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "asio/buffer.hpp"
#include "asio/ip/address.hpp"
#include "asio/ssl/host_name_verification.hpp"
#include "asio/ssl/stream_base.hpp"
#include "asio/write.hpp"

namespace {

// A player, the way the client is one: over TLS, with a token. It keeps the
// last world the server sent.
class TestPlayer {
 public:
  TestPlayer(asio::io_context& io, asio::ssl::context& tls, uint16_t port,
             std::string token)
      : stream_(io, tls) {
    stream_.set_verify_callback(asio::ssl::host_name_verification("localhost"));
    stream_.lowest_layer().async_connect(
        {asio::ip::make_address("127.0.0.1"), port},
        [this, token = std::move(token)](std::error_code error) {
          if (error) {
            closed_ = true;
            return;
          }
          stream_.async_handshake(
              asio::ssl::stream_base::client,
              [this, token](std::error_code error) { Greet(token, error); });
        });
  }

  // Whether the last world the server sent has the player in it.
  bool Sees(uint32_t id) const {
    return std::ranges::any_of(world_.players(),
                               [id](const nanommo::protocol::Player& player) {
                                 return player.id() == id;
                               });
  }

  const nanommo::protocol::World& world() const { return world_; }

  bool closed() const { return closed_; }

 private:
  void Greet(const std::string& token, std::error_code error) {
    if (error) {
      closed_ = true;
      return;
    }
    nanommo::protocol::Hello hello;
    hello.set_token(token);
    hello_ = nanommo::Frame(hello);
    asio::async_write(stream_, asio::buffer(hello_),
                      [](std::error_code, std::size_t) {});
    Read();
  }

  void Read() {
    stream_.async_read_some(
        asio::buffer(buffer_), [this](std::error_code error, std::size_t size) {
          if (error) {
            closed_ = true;
            return;
          }
          reader_.Append({buffer_.data(), size});
          while (std::optional<std::string> message = reader_.Next()) {
            nanommo::protocol::World world;
            // Chat comes in worlds of its own, with nobody in them.
            if (world.ParseFromString(*message) && world.chat().empty()) {
              world_ = std::move(world);
            }
          }
          Read();
        });
  }

  nanommo::TlsStream stream_;
  std::string hello_;
  std::array<char, 4096> buffer_;
  nanommo::FrameReader reader_;
  nanommo::protocol::World world_;
  bool closed_ = false;
};

}  // namespace
