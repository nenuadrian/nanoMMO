#include "gateway/gateway.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "asio/buffer.hpp"
#include "asio/connect.hpp"
#include "asio/read_until.hpp"
#include "asio/ssl/stream_base.hpp"
#include "asio/steady_timer.hpp"
#include "asio/write.hpp"
#include "gateway/websocket.h"
#include "protocol/tls.h"

namespace nanommo {
namespace {

using asio::ip::tcp;

// An opening handshake is a few hundred bytes; one this long is something else.
constexpr std::size_t kMaxRequest = 8 * 1024;

// A page that's silent this long has gone: the client sends its keys each
// tick in a world, and a login waits a moment for its answer.
constexpr std::chrono::seconds kIdle(60);

// What's waiting to go to a page that reads too slowly, before it's dropped.
constexpr std::size_t kMaxQueued = 4 * 1024 * 1024;

// RFC 6455's close codes: done, gone away, and the protocol broken.
constexpr uint16_t kNormal = 1000;
constexpr uint16_t kGoingAway = 1001;
constexpr uint16_t kProtocolError = 1002;

std::string CloseBody(uint16_t code, std::string_view reason) {
  return absl::StrCat(
      std::string{static_cast<char>(code >> 8), static_cast<char>(code & 0xFF)},
      reason);
}

int HttpStatus(const absl::Status& status) {
  switch (status.code()) {
    case absl::StatusCode::kNotFound:
      return 404;
    case absl::StatusCode::kPermissionDenied:
      return 403;
    case absl::StatusCode::kUnavailable:
      return 502;
    default:
      return 400;
  }
}

// One page's connection: its handshake, then its messages to the server it
// asked for, and that server's bytes back, until either side goes.
class Session : public std::enable_shared_from_this<Session> {
 public:
  Session(tcp::socket page, asio::ssl::context& tls, const Route& route)
      : page_(std::move(page)),
        upstream_(page_.get_executor(), tls),
        resolver_(page_.get_executor()),
        idle_(page_.get_executor()),
        route_(route) {
    std::error_code ignored;
    const tcp::endpoint peer = page_.remote_endpoint(ignored);
    peer_ = absl::StrCat(peer.address().to_string(), ":", peer.port());
  }

  void Start() { ReadHandshake(); }

 private:
  void ReadHandshake() {
    Idle();
    asio::async_read_until(
        page_, asio::dynamic_buffer(request_, kMaxRequest), "\r\n\r\n",
        [this, self = shared_from_this()](std::error_code error,
                                          std::size_t size) {
          if (error) {
            Close("the page went before its handshake");
            return;
          }
          const std::string head = request_.substr(0, size);
          // Anything after the handshake is the page's first frames.
          decoder_.Append(std::string_view(request_).substr(size));
          request_.clear();
          absl::StatusOr<Handshake> handshake = ParseHandshake(head);
          if (!handshake.ok()) {
            TurnDown(handshake.status());
            return;
          }
          path_ = handshake->path;
          absl::StatusOr<Upstream> upstream = route_(handshake->path);
          if (!upstream.ok()) {
            TurnDown(upstream.status());
            return;
          }
          Connect(*upstream, handshake->key);
        });
  }

  void Connect(const Upstream& upstream, const std::string& key) {
    resolver_.async_resolve(
        upstream.host, std::to_string(upstream.port),
        [this, self = shared_from_this(), upstream, key](
            std::error_code error, const tcp::resolver::results_type& found) {
          if (error) {
            TurnDown(absl::UnavailableError(absl::StrCat(
                "couldn't find ", upstream.host, ": ", error.message())));
            return;
          }
          asio::async_connect(
              upstream_.lowest_layer(), found,
              [this, self, upstream, key](std::error_code error,
                                          const tcp::endpoint&) {
                if (error) {
                  TurnDown(absl::UnavailableError(
                      absl::StrCat("couldn't connect to ", upstream.host, ":",
                                   upstream.port, ": ", error.message())));
                  return;
                }
                std::error_code ignored;
                upstream_.lowest_layer().set_option(tcp::no_delay(true),
                                                    ignored);
                ShakeHands(upstream, key);
              });
        });
  }

  void ShakeHands(const Upstream& upstream, const std::string& key) {
    ExpectName(upstream_, upstream.name);
    upstream_.async_handshake(
        asio::ssl::stream_base::client, [this, self = shared_from_this(),
                                         upstream, key](std::error_code error) {
          if (error) {
            TurnDown(absl::UnavailableError(
                absl::StrCat("couldn't shake hands with ", upstream.name, ": ",
                             HandshakeError(upstream_, upstream.name, error))));
            return;
          }
          ToPage(Accept(key));
          opened_ = std::chrono::steady_clock::now();
          relaying_ = true;
          std::cout << "nanommo gateway: " << peer_ << " opened " << path_
                    << ", to " << upstream.host << ":" << upstream.port
                    << std::endl;
          // The page's first frames may have come with its handshake.
          Relay();
          ReadPage();
          ReadUpstream();
        });
  }

  // An HTTP answer instead of the switch to WebSocket, then the connection
  // closes.
  void TurnDown(const absl::Status& why) {
    std::cerr << "nanommo gateway: turned " << peer_ << " down: " << why
              << "\n";
    closing_ = true;
    ToPage(Refuse(HttpStatus(why), why.message()));
  }

  void ReadPage() {
    page_.async_read_some(asio::buffer(page_buffer_),
                          [this, self = shared_from_this()](
                              std::error_code error, std::size_t size) {
                            if (error) {
                              Close("the page went");
                              return;
                            }
                            Idle();
                            decoder_.Append({page_buffer_.data(), size});
                            if (Relay()) {
                              ReadPage();
                            }
                          });
  }

  // Hands the page's messages on to the server, and answers its pings. False
  // once the page or the protocol has ended the connection.
  bool Relay() {
    while (std::optional<Message> message = decoder_.Next()) {
      switch (message->opcode) {
        case Opcode::kBinary:
        case Opcode::kText:
          ToUpstream(std::move(message->payload));
          break;
        case Opcode::kPing:
          ToPage(ServerFrame(Opcode::kPong, message->payload));
          break;
        case Opcode::kClose:
          closing_ = true;
          why_ = "the page closed it";
          ToPage(ServerFrame(Opcode::kClose, CloseBody(kNormal, "")));
          return false;
        default:
          break;
      }
    }
    if (!decoder_.status().ok()) {
      closing_ = true;
      why_ = std::string(decoder_.status().message());
      ToPage(
          ServerFrame(Opcode::kClose,
                      CloseBody(kProtocolError, decoder_.status().message())));
      return false;
    }
    return true;
  }

  void ReadUpstream() {
    upstream_.async_read_some(
        asio::buffer(upstream_buffer_),
        [this, self = shared_from_this()](std::error_code error,
                                          std::size_t size) {
          if (error) {
            // The server went, so the page goes too.
            closing_ = true;
            why_ = "the server closed it";
            ToPage(ServerFrame(Opcode::kClose,
                               CloseBody(kGoingAway, "the server closed")));
            return;
          }
          // The server's bytes as they come, which the page puts back
          // together into the game's messages, as it would from TCP.
          ToPage(ServerFrame(Opcode::kBinary, {upstream_buffer_.data(), size}));
          ReadUpstream();
        });
  }

  void ToPage(std::string bytes) {
    if (closed_) {
      return;
    }
    if (to_page_size_ + bytes.size() > kMaxQueued) {
      Close("the page couldn't keep up");
      return;
    }
    to_page_size_ += bytes.size();
    to_page_.push_back(std::move(bytes));
    if (to_page_.size() == 1) {
      WritePage();
    }
  }

  void WritePage() {
    asio::async_write(
        page_, asio::buffer(to_page_.front()),
        [this, self = shared_from_this()](std::error_code error, std::size_t) {
          if (error) {
            Close("the page went");
            return;
          }
          to_page_size_ -= to_page_.front().size();
          to_page_.pop_front();
          if (!to_page_.empty()) {
            WritePage();
          } else if (closing_) {
            Close(why_);
          }
        });
  }

  void ToUpstream(std::string bytes) {
    if (closed_) {
      return;
    }
    to_upstream_.push_back(std::move(bytes));
    if (to_upstream_.size() == 1) {
      WriteUpstream();
    }
  }

  void WriteUpstream() {
    asio::async_write(
        upstream_, asio::buffer(to_upstream_.front()),
        [this, self = shared_from_this()](std::error_code error, std::size_t) {
          if (error) {
            Close("couldn't write to the server");
            return;
          }
          to_upstream_.pop_front();
          if (!to_upstream_.empty()) {
            WriteUpstream();
          }
        });
  }

  void Idle() {
    idle_.expires_after(kIdle);
    idle_.async_wait([this, self = shared_from_this()](std::error_code error) {
      if (!error) {
        Close("the page was silent for a minute");
      }
    });
  }

  // Closes both sides, and says why, for a page that got as far as the
  // server; a page turned down was said already.
  void Close(std::string_view why) {
    if (closed_) {
      return;
    }
    closed_ = true;
    if (relaying_) {
      const std::chrono::duration<double> lasted =
          std::chrono::steady_clock::now() - opened_;
      std::cout << "nanommo gateway: " << peer_ << " closed " << path_
                << " after " << absl::StrFormat("%.1f", lasted.count())
                << " s: " << why << std::endl;
    }
    std::error_code ignored;
    idle_.cancel();
    resolver_.cancel();
    page_.close(ignored);
    upstream_.lowest_layer().close(ignored);
  }

  tcp::socket page_;
  TlsStream upstream_;
  tcp::resolver resolver_;
  asio::steady_timer idle_;
  const Route& route_;
  std::string request_;
  FrameDecoder decoder_{Gateway::kMaxMessage};
  std::array<char, 8192> page_buffer_{};
  std::array<char, 8192> upstream_buffer_{};
  std::deque<std::string> to_page_;
  std::size_t to_page_size_ = 0;
  std::deque<std::string> to_upstream_;
  std::string peer_;
  std::string path_;
  std::chrono::steady_clock::time_point opened_;
  bool relaying_ = false;
  std::string why_;
  bool closing_ = false;
  bool closed_ = false;
};

}  // namespace

Route Routes(Upstream login, WorldAddress world_address) {
  return [login = std::move(login), world_address = std::move(world_address)](
             std::string_view path) -> absl::StatusOr<Upstream> {
    path = path.substr(0, path.find('?'));
    if (path == "/login") {
      return login;
    }
    const std::vector<std::string_view> parts = absl::StrSplit(path, '/');
    if (parts.size() == 4 && parts[0].empty() && parts[1] == "world") {
      uint32_t port = 0;
      if (!absl::SimpleAtoi(parts[3], &port) || port == 0 || port > 65535) {
        return absl::InvalidArgumentError(
            absl::StrCat(parts[3], " isn't a port"));
      }
      absl::StatusOr<std::string> address =
          world_address(parts[2], static_cast<uint16_t>(port));
      if (!address.ok()) {
        return address.status();
      }
      // Connected at its own address, it's checked by the name players use.
      return Upstream{.host = *std::move(address),
                      .port = static_cast<uint16_t>(port),
                      .name = std::string(parts[2])};
    }
    return absl::NotFoundError(absl::StrCat("nothing at ", path));
  };
}

absl::StatusOr<std::unique_ptr<Gateway>> Gateway::Create(
    asio::io_context& io, uint16_t port, asio::ssl::context& tls, Route route) {
  std::unique_ptr<Gateway> gateway =
      absl::WrapUnique(new Gateway(io, tls, std::move(route)));
  std::error_code error;
  const tcp::endpoint endpoint(tcp::v4(), port);
  gateway->acceptor_.open(endpoint.protocol(), error);
  if (!error) {
    gateway->acceptor_.set_option(tcp::acceptor::reuse_address(true), error);
  }
  if (!error) {
    gateway->acceptor_.bind(endpoint, error);
  }
  if (!error) {
    gateway->acceptor_.listen(asio::socket_base::max_listen_connections, error);
  }
  if (error) {
    return absl::UnavailableError(
        absl::StrCat("couldn't listen on port ", port, ": ", error.message()));
  }
  gateway->Accept();
  return gateway;
}

Gateway::Gateway(asio::io_context& io, asio::ssl::context& tls, Route route)
    : acceptor_(io), tls_(tls), route_(std::move(route)) {}

void Gateway::Accept() {
  acceptor_.async_accept([this](std::error_code error, tcp::socket socket) {
    if (!error) {
      std::make_shared<Session>(std::move(socket), tls_, route_)->Start();
    }
    Accept();
  });
}

}  // namespace nanommo
