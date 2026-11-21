#ifndef NANOMMO_SERVER_WARS_H_
#define NANOMMO_SERVER_WARS_H_

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

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

  /// The other side from the guild's, by its id.
  uint32_t Against(uint32_t guild) const {
    return guild == challenger ? challenged : challenger;
  }
};

/// A player killed by an enemy at war, for the war's score: the war, the
/// killer and the one they killed, by their ids, and the killer's guild, whose
/// side it counts for. The server gives it an id of its own, so that the saver
/// records it once, however many times it's handed it.
struct WarKill {
  std::string id;
  uint32_t war = 0;
  uint32_t killer = 0;
  uint32_t victim = 0;
  uint32_t guild = 0;
};

/// Which war kills score: a victim scores for a war once every five minutes
/// at most, so that a player can't run up their guild's score on a friend in
/// the other guild who keeps coming back to die. The kill happens either way.
/// It's the server's to remember, so an instance starts with none.
class Scoring {
 public:
  static constexpr std::chrono::minutes kOnceEvery{5};

  /// Whether the kill, now, scores. One that does is remembered for
  /// kOnceEvery.
  bool Scores(const WarKill& kill, std::chrono::steady_clock::time_point now) {
    // Those who scored long enough ago can score again.
    std::erase_if(scored_, [now](const auto& victim) {
      return now - victim.second >= kOnceEvery;
    });
    return scored_.try_emplace({kill.war, kill.victim}, now).second;
  }

 private:
  /// When each victim last scored, by the war and their id.
  std::map<std::pair<uint32_t, uint32_t>, std::chrono::steady_clock::time_point>
      scored_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_WARS_H_
