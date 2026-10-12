#ifndef NANOMMO_SERVER_SESSION_H_
#define NANOMMO_SERVER_SESSION_H_

#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <string>

#include "asio/ip/tcp.hpp"
#include "asio/ssl/context.hpp"
#include "protocol/framing.h"
#include "protocol/tls.h"

namespace nanommo {

class Session : public std::enable_shared_from_this<Session> {
 public:
  using OnMessage = std::function<void(Session&, const std::string&)>;
  using OnClose = std::function<void()>;

  Session(asio::ip::tcp::socket socket, asio::ssl::context& tls,
          OnMessage on_message, OnClose on_close);

  /// Shakes hands over TLS, then reads the client's messages.
  void Start();

  void Send(std::string bytes);

  void Send(std::shared_ptr<const std::string> shared, std::string own);

  void Close();

 private:
  struct Outgoing {
    std::string head;
    std::shared_ptr<const std::string> shared;
    std::string tail;
  };

  void Read();
  void Send(Outgoing outgoing);
  void Write();

  TlsStream stream_;
  OnMessage on_message_;
  OnClose on_close_;
  std::array<char, 4096> buffer_;
  FrameReader reader_;
  std::deque<Outgoing> outbox_;
  bool closed_ = false;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_SESSION_H_
