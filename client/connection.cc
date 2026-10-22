#include "client/connection.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "asio/buffer.hpp"
#include "asio/connect.hpp"
#include "asio/ssl/host_name_verification.hpp"
#include "asio/ssl/stream_base.hpp"
#include "asio/write.hpp"

namespace nanommo {

namespace {

constexpr std::size_t kChatLines = 100;

}  // namespace

absl::StatusOr<std::unique_ptr<Connection>> Connection::Create(
    const std::string& host, uint16_t port, asio::ssl::context& tls,
    const std::string& token, bool udp) {
  std::unique_ptr<Connection> connection =
      absl::WrapUnique(new Connection(tls, token));
  const absl::Status started = connection->Start(host, port, token, udp);
  if (!started.ok()) {
    return started;
  }
  return connection;
}

Connection::Connection(asio::ssl::context& tls, const std::string& token)
    : stream_(io_, tls), udp_socket_(io_), hello_timer_(io_), token_(token) {}

absl::Status Connection::Start(const std::string& host, uint16_t port,
                               const std::string& token, bool udp) {
  asio::ip::tcp::resolver resolver(io_);
  std::error_code error;
  const asio::ip::tcp::resolver::results_type endpoints =
      resolver.resolve(host, std::to_string(port), error);
  if (!error) {
    asio::connect(stream_.lowest_layer(), endpoints, error);
  }
  if (error) {
    return absl::UnavailableError(absl::StrCat(
        "couldn't connect to ", host, ":", port, ": ", error.message()));
  }
  std::error_code ignored;
  stream_.lowest_layer().set_option(asio::ip::tcp::no_delay(true), ignored);
  stream_.set_verify_callback(asio::ssl::host_name_verification(host));
  stream_.handshake(asio::ssl::stream_base::client, error);
  if (error) {
    return absl::UnavailableError(absl::StrCat(
        "couldn't shake hands with the server: ", error.message()));
  }
  protocol::Hello hello;
  hello.set_token(token);
  asio::write(stream_, asio::buffer(Frame(hello)), error);
  if (error) {
    return absl::UnavailableError(
        absl::StrCat("couldn't say hello to the server: ", error.message()));
  }
  Read();
  if (udp) {
    const asio::ip::tcp::endpoint server =
        stream_.lowest_layer().remote_endpoint(error);
    if (!error) {
      udp_socket_.connect({server.address(), server.port()}, error);
    }
    if (error) {
      return absl::UnavailableError(absl::StrCat(
          "couldn't reach the server over UDP: ", error.message()));
    }
    HelloOverUdp();
    ReceiveOverUdp();
  }
  return absl::OkStatus();
}

void Connection::Poll() { io_.poll(); }

void Connection::Send(const protocol::Input& input) {
  if (!connected_) {
    return;
  }
  std::error_code error;
  asio::write(stream_, asio::buffer(Frame(input)), error);
  if (error) {
    connected_ = false;
  }
}

void Connection::Read() {
  stream_.async_read_some(asio::buffer(buffer_),
                          [this](std::error_code error, std::size_t size) {
                            if (error) {
                              connected_ = false;
                              return;
                            }
                            reader_.Append({buffer_.data(), size});
                            while (const auto message = reader_.Next()) {
                              protocol::World world;
                              if (!world.ParseFromString(*message)) {
                                connected_ = false;
                                return;
                              }
                              // Chat and moves come on their own, with nothing
                              // of the world in them.
                              if (world.has_move()) {
                                move_ = world.move();
                              } else if (world.chat().empty()) {
                                Receive(world);
                              } else {
                                Hear(world);
                              }
                            }
                            if (!reader_.status().ok()) {
                              connected_ = false;
                              return;
                            }
                            Read();
                          });
}

void Connection::HelloOverUdp() {
  if (udp_worlds_) {
    return;
  }
  protocol::Hello hello;
  hello.set_token(token_);
  std::error_code ignored;
  udp_socket_.send(asio::buffer(hello.SerializeAsString()), 0, ignored);
  hello_timer_.expires_after(std::chrono::milliseconds(250));
  hello_timer_.async_wait([this](std::error_code error) {
    if (!error) {
      HelloOverUdp();
    }
  });
}

void Connection::ReceiveOverUdp() {
  udp_socket_.async_receive(
      asio::buffer(datagram_), [this](std::error_code error, std::size_t size) {
        protocol::World part;
        if (!error &&
            part.ParseFromArray(datagram_.data(), static_cast<int>(size))) {
          udp_worlds_ = true;
          Receive(part);
        }
        ReceiveOverUdp();
      });
}

void Connection::Receive(const protocol::World& world) {
  parts_.Add(world);
  world_ = parts_.world();
}

void Connection::Hear(const protocol::World& chat) {
  chat_.insert(chat_.end(), chat.chat().begin(), chat.chat().end());
  while (chat_.size() > kChatLines) {
    chat_.pop_front();
  }
}

}  // namespace nanommo
