#ifndef NANOMMO_SERVER_WARS_H_
#define NANOMMO_SERVER_WARS_H_

#include <chrono>
#include <cstdint>
#include <string>

namespace nanommo {

/// A war between two guilds of a world, as MySQL has it: the guild that
/// asked, the one that said yes, by their ids and names, how many of the
/// other side's players each has killed, and how long it has left. A guild is
/// in one war at a time.
struct War {
  uint32_t id = 0;
  uint32_t challenger = 0;
  std::string challenger_name;
  uint32_t challenged = 0;
  std::string challenged_name;
  uint32_t challenger_kills = 0;
  uint32_t challenged_kills = 0;
  std::chrono::seconds left{0};
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_WARS_H_
