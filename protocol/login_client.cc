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
#include "asio/ssl/host_name_verification.hpp"
#include "asio/ssl/stream_base.hpp"
#include "asio/write.hpp"
#include "protocol/framing.h"
#include "protocol/tls.h"

namespace nanommo {

absl::StatusOr<protocol::LoginReply> LogIn(
    const std::string& host, uint16_t port, asio::ssl::context& tls,
    const protocol::LoginRequest& request) {
  asio::io_context io;
  TlsStream stream(io, tls);
  asio::ip::tcp::resolver resolver(io);
  std::error_code error;
  const asio::ip::tcp::resolver::results_type endpoints =
      resolver.resolve(host, std::to_string(port), error);
  if (!error) {
    asio::connect(stream.lowest_layer(), endpoints, error);
  }
  if (!error) {
    stream.set_verify_callback(asio::ssl::host_name_verification(host));
    stream.handshake(asio::ssl::stream_base::client, error);
  }
  if (!error) {
    asio::write(stream, asio::buffer(Frame(request)), error);
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
    const std::size_t size = stream.read_some(asio::buffer(buffer), error);
    reader.Append({buffer.data(), size});
  }
  return absl::UnavailableError(
      absl::StrCat("couldn't reach the login server: ", error.message()));
}

}  // namespace nanommo
