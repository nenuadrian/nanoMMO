#include "protocol/login_client.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "asio/buffer.hpp"
#include "asio/connect.hpp"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/ssl/stream_base.hpp"
#include "asio/write.hpp"
#include "protocol/framing.h"
#include "protocol/tls.h"

namespace nanommo {

absl::StatusOr<protocol::LoginReply> LogIn(
    const std::string& host, uint16_t port, asio::ssl::context& tls,
    const protocol::LoginRequest& request, std::chrono::milliseconds deadline) {
  // Each step waits for the one before, on this io_context, which runs until
  // there's an answer, or until the deadline. Whatever's still waiting then
  // goes with it, never called.
  asio::io_context io;
  TlsStream stream(io, tls);
  asio::ip::tcp::resolver resolver(io);
  const std::string frame = Frame(request);
  FrameReader reader;
  std::array<char, 1024> buffer;
  std::optional<absl::StatusOr<protocol::LoginReply>> answer;
  const auto unreachable = [&answer](const std::error_code& error) {
    answer = absl::UnavailableError(
        absl::StrCat("couldn't reach the login server: ", error.message()));
  };

  std::function<void()> read = [&] {
    stream.async_read_some(
        asio::buffer(buffer), [&](std::error_code error, std::size_t size) {
          if (error) {
            unreachable(error);
            return;
          }
          reader.Append({buffer.data(), size});
          if (const std::optional<std::string> message = reader.Next()) {
            protocol::LoginReply reply;
            if (!reply.ParseFromString(*message)) {
              answer =
                  absl::DataLossError("couldn't read the login server's reply");
              return;
            }
            answer = std::move(reply);
          } else if (!reader.status().ok()) {
            answer = reader.status();
          } else {
            read();
          }
        });
  };
  resolver.async_resolve(
      host, std::to_string(port),
      [&](std::error_code error,
          const asio::ip::tcp::resolver::results_type& endpoints) {
        if (error) {
          unreachable(error);
          return;
        }
        asio::async_connect(
            stream.lowest_layer(), endpoints,
            [&](std::error_code error, const asio::ip::tcp::endpoint&) {
              if (error) {
                unreachable(error);
                return;
              }
              ExpectName(stream, host);
              stream.async_handshake(
                  asio::ssl::stream_base::client, [&](std::error_code error) {
                    if (error) {
                      answer = absl::UnavailableError(absl::StrCat(
                          "couldn't shake hands with the login server: ",
                          HandshakeError(stream, host, error)));
                      return;
                    }
                    asio::async_write(stream, asio::buffer(frame),
                                      [&](std::error_code error, std::size_t) {
                                        if (error) {
                                          unreachable(error);
                                          return;
                                        }
                                        read();
                                      });
                  });
            });
      });
  io.run_for(deadline);
  if (!answer) {
    return absl::DeadlineExceededError(
        absl::StrCat("the login server didn't answer within ",
                     absl::FormatDuration(absl::FromChrono(deadline))));
  }
  return *std::move(answer);
}

}  // namespace nanommo
