#ifndef NANOMMO_SERVER_SESSION_H_
#define NANOMMO_SERVER_SESSION_H_

#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <string>

#include "asio/ip/tcp.hpp"
#include "protocol/framing.h"

namespace nanommo {

class Session : public std::enable_shared_from_this<Session> {
 public:
  using OnMessage = std::function<void(Session&, const std::string&)>;
  using OnClose = std::function<void()>;

  Session(asio::ip::tcp::socket socket, OnMessage on_message, OnClose on_close);

  void Start();

  void Send(std::string bytes);

  void Close();

 private:
  void Read();
  void Write();

  asio::ip::tcp::socket socket_;
  OnMessage on_message_;
  OnClose on_close_;
  std::array<char, 4096> buffer_;
  FrameReader reader_;
  std::deque<std::string> outbox_;
  bool closed_ = false;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_SESSION_H_
