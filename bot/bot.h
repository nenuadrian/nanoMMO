#ifndef NANOMMO_BOT_BOT_H_
#define NANOMMO_BOT_BOT_H_

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/ip/udp.hpp"
#include "asio/ssl/context.hpp"
#include "asio/steady_timer.hpp"
#include "protocol/framing.h"
#include "protocol/tls.h"

namespace nanommo {

class Bot {
 public:
  Bot(asio::io_context& io, asio::ssl::context& tls, uint32_t seed, bool udp);

  Bot(const Bot&) = delete;
  Bot& operator=(const Bot&) = delete;

  void Start(const asio::ip::tcp::resolver::results_type& server,
             const std::string& token);

  bool connected() const { return connected_; }

  const std::vector<int64_t>& newest() const { return newest_; }

 private:
  void Greet(const std::string& token, std::error_code error);
  void Read();
  void HelloOverUdp();
  void ReceiveOverUdp();
  void Receive(std::string_view bytes);
  void Wander();
  void Stop();

  TlsStream stream_;
  asio::ip::udp::socket udp_socket_;
  asio::steady_timer timer_;
  asio::steady_timer hello_timer_;
  std::mt19937 random_;
  bool udp_;
  std::string token_;
  std::array<char, 65536> buffer_;
  std::array<char, 2048> datagram_;
  FrameReader reader_;
  std::vector<int64_t> newest_;
  bool udp_worlds_ = false;
  std::string outgoing_;
  bool connected_ = false;
};

}  // namespace nanommo

#endif  // NANOMMO_BOT_BOT_H_
