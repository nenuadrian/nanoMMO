#ifndef NANOMMO_BOT_BACKOFF_H_
#define NANOMMO_BOT_BACKOFF_H_

#include <chrono>
#include <random>

namespace nanommo {

/// How long a bot waits to log in again, after `failed` tries that failed in
/// a row: a second, twice that after each failure, up to thirty, and of that
/// a random part, from half to all of it, so that bots dropped together don't
/// all come back together, each time.
std::chrono::milliseconds Backoff(int failed, std::mt19937& random);

}  // namespace nanommo

#endif  // NANOMMO_BOT_BACKOFF_H_
