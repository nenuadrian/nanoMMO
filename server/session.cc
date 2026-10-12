#include "server/session.h"

#include <array>
#include <string>
#include <utility>

#include "asio/buffer.hpp"
#include "asio/ssl/stream_base.hpp"
#include "asio/write.hpp"
#include "tracy/Tracy.hpp"

namespace nanommo {

Session::Session(asio::ip::tcp::socket socket, asio::ssl::context& tls,
                 OnMessage on_message, OnClose on_close)
    : stream_(std::move(socket), tls),
      on_message_(std::move(on_message)),
      on_close_(std::move(on_close)) {
  // Turn off Nagle's algorithm, so small messages go out straight away.
  std::error_code ignored;
  stream_.lowest_layer().set_option(asio::ip::tcp::no_delay(true), ignored);
}

void Session::Start() {
  stream_.async_handshake(asio::ssl::stream_base::server,
                          [self = shared_from_this()](std::error_code error) {
                            if (error) {
                              self->Close();
                              return;
                            }
                            self->Read();
                          });
}

void Session::Read() {
  // Capturing self, not this, keeps the session alive until the read finishes.
  stream_.async_read_some(
      asio::buffer(buffer_),
      [self = shared_from_this()](std::error_code error, std::size_t size) {
        if (error) {
          self->Close();
          return;
        }
        self->reader_.Append({self->buffer_.data(), size});
        while (const auto message = self->reader_.Next()) {
          self->on_message_(*self, *message);
          if (self->closed_) {
            return;
          }
        }
        if (!self->reader_.status().ok()) {
          self->Close();
          return;
        }
        self->Read();
      });
}

void Session::Send(std::string bytes) {
  Send(Outgoing{std::move(bytes), nullptr, {}});
}

void Session::Send(std::shared_ptr<const std::string> shared, std::string own) {
  const std::size_t size = shared->size() + own.size();
  Send(Outgoing{Header(size), std::move(shared), std::move(own)});
}

void Session::Send(Outgoing outgoing) {
  ZoneScoped;
  if (closed_) {
    return;
  }
  outbox_.push_back(std::move(outgoing));
  // Writes mustn't overlap, so if one is under way, it picks these up next.
  if (outbox_.size() == 1) {
    Write();
  }
}

void Session::Write() {
  const Outgoing& next = outbox_.front();
  const std::array buffers{
      asio::buffer(next.head),
      next.shared ? asio::buffer(*next.shared) : asio::const_buffer(),
      asio::buffer(next.tail)};
  asio::async_write(
      stream_, buffers,
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
  stream_.lowest_layer().close(ignored);
  on_close_();
}

}  // namespace nanommo
