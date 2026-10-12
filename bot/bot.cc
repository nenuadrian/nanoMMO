#include "bot/bot.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>

#include "asio/buffer.hpp"
#include "asio/connect.hpp"
#include "asio/error.hpp"
#include "asio/ssl/host_name_verification.hpp"
#include "asio/ssl/stream_base.hpp"
#include "asio/write.hpp"
#include "bot/wander.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

Bot::Bot(asio::io_context& io, asio::ssl::context& tls, uint32_t seed, bool udp)
    : stream_(io, tls),
      udp_socket_(io),
      timer_(io),
      hello_timer_(io),
      random_(seed),
      udp_(udp) {}

void Bot::Start(const asio::ip::tcp::resolver::results_type& server,
                const std::string& token) {
  // The resolver keeps the name we looked up, which the certificate must have.
  stream_.set_verify_callback(
      asio::ssl::host_name_verification(server.begin()->host_name()));
  asio::async_connect(
      stream_.lowest_layer(), server,
      [this, token](std::error_code error, const auto&) {
        if (error) {
          std::cerr << "nanommo bot: couldn't connect: " << error.message()
                    << '\n';
          return;
        }
        std::error_code ignored;
        stream_.lowest_layer().set_option(asio::ip::tcp::no_delay(true),
                                          ignored);
        stream_.async_handshake(
            asio::ssl::stream_base::client,
            [this, token](std::error_code error) { Greet(token, error); });
      });
}

void Bot::Greet(const std::string& token, std::error_code error) {
  if (error) {
    std::cerr << "nanommo bot: couldn't shake hands: " << error.message()
              << '\n';
    return;
  }
  // Blocking is fine: a hello this small fits in the socket's buffer.
  protocol::Hello hello;
  hello.set_token(token);
  asio::write(stream_, asio::buffer(Frame(hello)), error);
  if (error) {
    return;
  }
  connected_ = true;
  Read();
  Wander();
  if (udp_) {
    const asio::ip::tcp::endpoint server =
        stream_.lowest_layer().remote_endpoint(error);
    if (!error) {
      udp_socket_.connect({server.address(), server.port()}, error);
    }
    if (!error) {
      token_ = token;
      HelloOverUdp();
      ReceiveOverUdp();
    }
  }
}

// Reading every world keeps the server's writes to us from backing up.
void Bot::Read() {
  stream_.async_read_some(asio::buffer(buffer_),
                          [this](std::error_code error, std::size_t size) {
                            if (error) {
                              Stop();
                              return;
                            }
                            reader_.Append({buffer_.data(), size});
                            while (const auto message = reader_.Next()) {
                              Receive(*message);
                            }
                            if (!reader_.status().ok()) {
                              Stop();
                              return;
                            }
                            Read();
                          });
}

void Bot::HelloOverUdp() {
  if (!connected_ || udp_worlds_) {
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

void Bot::ReceiveOverUdp() {
  udp_socket_.async_receive(
      asio::buffer(datagram_), [this](std::error_code error, std::size_t size) {
        if (!connected_ || error == asio::error::operation_aborted) {
          return;
        }
        if (!error) {
          udp_worlds_ = true;
          Receive({datagram_.data(), size});
        }
        ReceiveOverUdp();
      });
}

void Bot::Receive(std::string_view bytes) {
  protocol::World world;
  if (!world.ParseFromArray(bytes.data(), static_cast<int>(bytes.size()))) {
    return;
  }
  const uint32_t parts = std::max(world.parts(), 1u);
  newest_.resize(parts);
  if (world.part() < parts) {
    newest_[world.part()] = std::max(newest_[world.part()], world.sent());
  }
}

void Bot::Wander() {
  outgoing_ = Frame(RandomDirection(random_));
  asio::async_write(stream_, asio::buffer(outgoing_),
                    [this](std::error_code error, std::size_t) {
                      if (error) {
                        connected_ = false;
                      }
                    });
  timer_.expires_after(RandomHold(random_));
  timer_.async_wait([this](std::error_code error) {
    if (!error && connected_) {
      Wander();
    }
  });
}

void Bot::Stop() {
  connected_ = false;
  timer_.cancel();
  hello_timer_.cancel();
  std::error_code ignored;
  udp_socket_.close(ignored);
}

}  // namespace nanommo
