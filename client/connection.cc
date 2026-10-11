#include "client/connection.h"

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "asio/buffer.hpp"
#include "asio/connect.hpp"
#include "asio/write.hpp"

namespace nanommo {

absl::StatusOr<std::unique_ptr<Connection>> Connection::Create(
    const std::string& host, uint16_t port, const std::string& token) {
  std::unique_ptr<Connection> connection = absl::WrapUnique(new Connection());
  const absl::Status started = connection->Start(host, port, token);
  if (!started.ok()) {
    return started;
  }
  return connection;
}

Connection::Connection() : socket_(io_) {}

absl::Status Connection::Start(const std::string& host, uint16_t port,
                               const std::string& token) {
  asio::ip::tcp::resolver resolver(io_);
  std::error_code error;
  const asio::ip::tcp::resolver::results_type endpoints =
      resolver.resolve(host, std::to_string(port), error);
  if (!error) {
    asio::connect(socket_, endpoints, error);
  }
  if (error) {
    return absl::UnavailableError(absl::StrCat(
        "couldn't connect to ", host, ":", port, ": ", error.message()));
  }
  std::error_code ignored;
  socket_.set_option(asio::ip::tcp::no_delay(true), ignored);
  protocol::Hello hello;
  hello.set_token(token);
  asio::write(socket_, asio::buffer(Frame(hello)), error);
  if (error) {
    return absl::UnavailableError(
        absl::StrCat("couldn't say hello to the server: ", error.message()));
  }
  Read();
  return absl::OkStatus();
}

void Connection::Poll() { io_.poll(); }

void Connection::Send(const protocol::Input& input) {
  if (!connected_) {
    return;
  }
  std::error_code error;
  asio::write(socket_, asio::buffer(Frame(input)), error);
  if (error) {
    connected_ = false;
  }
}

void Connection::Read() {
  socket_.async_read_some(asio::buffer(buffer_),
                          [this](std::error_code error, std::size_t size) {
                            if (error) {
                              connected_ = false;
                              return;
                            }
                            reader_.Append({buffer_.data(), size});
                            while (const auto message = reader_.Next()) {
                              if (!world_.ParseFromString(*message)) {
                                connected_ = false;
                                return;
                              }
                            }
                            if (!reader_.status().ok()) {
                              connected_ = false;
                              return;
                            }
                            Read();
                          });
}

}  // namespace nanommo
