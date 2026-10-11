#ifndef NANOMMO_BOT_BOT_H_
#define NANOMMO_BOT_BOT_H_

#include <array>
#include <cstdint>
#include <random>
#include <string>

#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/steady_timer.hpp"

namespace nanommo {

class Bot {
 public:
  Bot(asio::io_context& io, uint32_t seed);

  Bot(const Bot&) = delete;
  Bot& operator=(const Bot&) = delete;

  void Start(const asio::ip::tcp::resolver::results_type& server,
             const std::string& token);

  bool connected() const { return connected_; }

 private:
  void Read();
  void Wander();

  asio::ip::tcp::socket socket_;
  asio::steady_timer timer_;
  std::mt19937 random_;
  std::array<char, 65536> buffer_;
  std::string outgoing_;
  bool connected_ = false;
};

}  // namespace nanommo

#endif  // NANOMMO_BOT_BOT_H_
