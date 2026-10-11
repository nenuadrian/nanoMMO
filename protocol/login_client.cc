#include "protocol/login_client.h"

#include <array>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "asio/buffer.hpp"
#include "asio/connect.hpp"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/write.hpp"
#include "protocol/framing.h"

namespace nanommo {

absl::StatusOr<protocol::LoginReply> LogIn(
    const std::string& host, uint16_t port,
    const protocol::LoginRequest& request) {
  asio::io_context io;
  asio::ip::tcp::socket socket(io);
  asio::ip::tcp::resolver resolver(io);
  std::error_code error;
  const asio::ip::tcp::resolver::results_type endpoints =
      resolver.resolve(host, std::to_string(port), error);
  if (!error) {
    asio::connect(socket, endpoints, error);
  }
  if (!error) {
    asio::write(socket, asio::buffer(Frame(request)), error);
  }

  FrameReader reader;
  std::array<char, 1024> buffer;
  while (!error) {
    if (const std::optional<std::string> message = reader.Next()) {
      protocol::LoginReply reply;
      if (!reply.ParseFromString(*message)) {
        return absl::DataLossError("couldn't read the login server's reply");
      }
      return reply;
    }
    if (!reader.status().ok()) {
      return reader.status();
    }
    const std::size_t size = socket.read_some(asio::buffer(buffer), error);
    reader.Append({buffer.data(), size});
  }
  return absl::UnavailableError(
      absl::StrCat("couldn't reach the login server: ", error.message()));
}

}  // namespace nanommo
