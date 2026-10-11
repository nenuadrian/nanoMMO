#include "server/session.h"

#include <string>
#include <utility>

#include "asio/buffer.hpp"
#include "asio/write.hpp"

namespace nanommo {

Session::Session(asio::ip::tcp::socket socket, OnMessage on_message,
                 OnClose on_close)
    : socket_(std::move(socket)),
      on_message_(std::move(on_message)),
      on_close_(std::move(on_close)) {
  // Turn off Nagle's algorithm, so small messages go out straight away.
  std::error_code ignored;
  socket_.set_option(asio::ip::tcp::no_delay(true), ignored);
}

void Session::Start() { Read(); }

void Session::Read() {
  // Capturing self, not this, keeps the session alive until the read finishes.
  socket_.async_read_some(
      asio::buffer(buffer_),
      [self = shared_from_this()](std::error_code error, std::size_t size) {
        if (error) {
          self->Close();
          return;
        }
        self->reader_.Append({self->buffer_.data(), size});
        while (const auto message = self->reader_.Next()) {
          self->on_message_(*message);
        }
        if (!self->reader_.status().ok()) {
          self->Close();
          return;
        }
        self->Read();
      });
}

void Session::Send(std::string bytes) {
  if (closed_) {
    return;
  }
  outbox_.push_back(std::move(bytes));
  // Writes mustn't overlap, so if one is under way, it picks these up next.
  if (outbox_.size() == 1) {
    Write();
  }
}

void Session::Write() {
  asio::async_write(
      socket_, asio::buffer(outbox_.front()),
      [self = shared_from_this()](std::error_code error, std::size_t) {
        if (error) {
          self->Close();
          return;
        }
        self->outbox_.pop_front();
        if (!self->outbox_.empty()) {
          self->Write();
        }
      });
}

void Session::Close() {
  if (closed_) {
    return;
  }
  closed_ = true;
  std::error_code ignored;
  socket_.close(ignored);
  on_close_();
}

}  // namespace nanommo
