#ifndef NANOMMO_GATEWAY_WORLD_LIST_H_
#define NANOMMO_GATEWAY_WORLD_LIST_H_

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "asio/io_context.hpp"
#include "asio/steady_timer.hpp"
#include "asio/thread_pool.hpp"

namespace nanommo {

/// A world instance, by the host and port players reach it at, and its own
/// address, where the gateway connects to it.
struct ListedWorld {
  std::string public_host;
  uint16_t port = 0;
  std::string address;
};

/// The worlds Consul lists, which the gateway may connect pages to. Asking
/// Consul waits on the network, so it's done on a thread of its own, once a
/// second, and the list it brings back is read and changed on the
/// io_context's thread alone, where the gateway runs: a page's route never
/// waits for Consul.
class WorldList {
 public:
  using Clock = std::chrono::steady_clock;
  using Fetch = std::function<absl::StatusOr<std::vector<ListedWorld>>()>;

  /// A list this old says nothing about now, so every world is refused.
  static constexpr std::chrono::seconds kStale{10};

  WorldList(asio::io_context& io, Fetch fetch);

  WorldList(const WorldList&) = delete;
  WorldList& operator=(const WorldList&) = delete;

  /// Destroy it once the io_context has stopped.
  ~WorldList();

  /// Asks for the list now, and every second after.
  void Start();

  /// A list that came back at the time, on the io_context's thread. An
  /// error keeps the list from before, which grows stale.
  void Take(absl::StatusOr<std::vector<ListedWorld>> worlds,
            Clock::time_point at);

  /// The world's own address: NotFound when it isn't on the list, and
  /// Unavailable when there's no list newer than kStale.
  absl::StatusOr<std::string> Find(std::string_view public_host, uint16_t port,
                                   Clock::time_point now) const;

 private:
  void Ask();

  asio::io_context& io_;
  Fetch fetch_;
  asio::steady_timer timer_;
  asio::thread_pool thread_{1};
  std::vector<ListedWorld> worlds_;
  std::optional<Clock::time_point> fetched_;
  bool asking_ = false;
};

}  // namespace nanommo

#endif  // NANOMMO_GATEWAY_WORLD_LIST_H_
