#ifndef NANOMMO_SERVER_STORYTELLER_H_
#define NANOMMO_SERVER_STORYTELLER_H_

#include <cstdint>
#include <functional>
#include <string>

#include "absl/status/statusor.h"
#include "asio/io_context.hpp"
#include "asio/thread_pool.hpp"

namespace nanommo {

/// Asks mvGPT's completions API, the storyteller service, for stories, on
/// threads of its own, so that a tick never waits for the model.
class Storyteller {
 public:
  static constexpr uint16_t kPort = 8000;

  /// Gets the story, or why there isn't one.
  using Told = std::function<void(absl::StatusOr<std::string>)>;

  Storyteller(asio::io_context& io, std::string host);

  /// Asks for a story that goes on from the prompt, and calls told with it on
  /// the io_context's thread, where the rest of the server runs.
  void Tell(std::string prompt, Told told);

 private:
  absl::StatusOr<std::string> Complete(const std::string& prompt) const;

  asio::io_context& io_;
  std::string host_;
  asio::thread_pool threads_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_STORYTELLER_H_
