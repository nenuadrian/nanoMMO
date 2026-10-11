#include "bot/bot.h"

#include <iostream>

#include "asio/buffer.hpp"
#include "asio/connect.hpp"
#include "asio/write.hpp"
#include "bot/wander.h"
#include "protocol/framing.h"

namespace nanommo {

Bot::Bot(asio::io_context& io, uint32_t seed)
    : socket_(io), timer_(io), random_(seed) {}

void Bot::Start(const asio::ip::tcp::resolver::results_type& server) {
  asio::async_connect(
      socket_, server, [this](std::error_code error, const auto&) {
        if (error) {
          std::cerr << "nanommo bot: couldn't connect: " << error.message()
                    << '\n';
          return;
        }
        std::error_code ignored;
        socket_.set_option(asio::ip::tcp::no_delay(true), ignored);
        connected_ = true;
        Read();
        Wander();
      });
}

// We ignore the world, but still read it so the server's writes don't back up.
void Bot::Read() {
  socket_.async_read_some(asio::buffer(buffer_),
                          [this](std::error_code error, std::size_t) {
                            if (error) {
                              connected_ = false;
                              timer_.cancel();
                              return;
                            }
                            Read();
                          });
}

void Bot::Wander() {
  outgoing_ = Frame(RandomDirection(random_));
  asio::async_write(socket_, asio::buffer(outgoing_),
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

}  // namespace nanommo
