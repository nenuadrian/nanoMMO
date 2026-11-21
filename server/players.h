#ifndef NANOMMO_SERVER_PLAYERS_H_
#define NANOMMO_SERVER_PLAYERS_H_

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "protocol/nanommo.pb.h"
#include "server/database.h"
#include "server/redis.h"
#include "server/world.h"

namespace nanommo {

/// What we keep of a player from one session to the next.
struct Progress {
  float x = 0.0f;
  float z = 0.0f;
  uint32_t experience = 0;
  int health = 0;
  int land = 0;
  /// What's in their main hand, which servers from before bags read, and the
  /// website might.
  uint32_t weapon = 0;
  std::set<uint32_t> skills;
  /// Their bag's revision, which goes up with each change to it, so that
  /// telling whether it needs saving takes a number, not the bag.
  uint32_t items_revision = 0;
  /// The quests they're on, with how many they've killed for each, and the
  /// ones they've done, by their ids.
  std::map<uint32_t, uint32_t> quests;
  std::set<uint32_t> quests_done;

  bool operator==(const Progress&) const = default;
};

/// A trade that's done, for the record: an id of its own, and each side, with
/// what they gave, serials and all.
struct Traded {
  std::string id;
  uint32_t player = 0;
  protocol::TradeSide player_gave;
  uint32_t partner = 0;
  protocol::TradeSide partner_gave;
};

class Players {
 public:
  static constexpr int kBatch = 500;
  /// The most rows of the bags that go in one statement.
  static constexpr std::size_t kItemRows = 1000;
  /// How long a lease on a player lasts, unless its instance renews it.
  static constexpr std::chrono::milliseconds kLease{10000};

  Players(Database& database, Redis& redis)
      : database_(database), redis_(redis) {}

  /// The character, by its id, in the world, from MySQL, with what Redis has
  /// on top, as that's newer. A NotFound error says there's no such character
  /// in this world.
  absl::StatusOr<Player> Load(const std::string& world, uint32_t id);

  /// Leases the character in the world to the instance, so that no other
  /// instance plays them until it's released or runs out, and returns the
  /// lease as Redis has it: the instance's id, and a fencing number, higher
  /// than any lease's before it. A lease handed to the instance in a handover
  /// is its already. FailedPrecondition, naming the instance, when another
  /// instance has it.
  absl::StatusOr<std::string> Lease(const std::string& world, uint32_t id,
                                    const std::string& instance);

  /// Keeps the leases, by player, for another kLease, all in one round trip:
  /// those that are still as given. Returns the players whose leases aren't.
  absl::StatusOr<std::set<uint32_t>> Renew(
      const std::string& world, const std::map<uint32_t, std::string>& leases);

  /// Ends the player's lease, if it's still as given, so that they can play
  /// on another instance at once. One that another instance has since stays.
  absl::Status Release(const std::string& world, uint32_t id,
                       const std::string& lease);

  /// Hands each of the leases, by player, that's still as given, to the next
  /// instance, with a new fencing number, all in one round trip. Returns the
  /// players whose leases it handed over.
  absl::StatusOr<std::set<uint32_t>> Pass(
      const std::string& world, const std::map<uint32_t, std::string>& leases,
      const std::string& next);

  /// Into Redis, all of them in a single round trip, and marked unsaved, with
  /// the bags of those whose bags changed, which are marked unsaved apart. It's
  /// one transaction, so Redis keeps all of it or none: the two bags a trade
  /// changed are never saved one without the other. And only while each of the
  /// leases, by player, is as given, so that an instance that's lost a player
  /// to another can't write over what that one saves. Returns the players
  /// whose leases aren't, when it saved none of it. A trade's record goes into
  /// the trades stream with its two bags, for the saver to take to MySQL with
  /// them, and each war kill into the war_kills stream, for its war's score.
  absl::StatusOr<std::set<uint32_t>> Save(
      const std::string& world, const std::map<uint32_t, Progress>& changed,
      const std::map<uint32_t, protocol::Inventory>& bags = {},
      const std::map<uint32_t, std::string>& leases = {},
      const std::optional<Traded>& traded = std::nullopt,
      const std::vector<WarKill>& kills = {});

  /// Takes the unsaved players of all the worlds from Redis to MySQL, a batch
  /// at a time, each in a transaction with the bags that changed, and returns
  /// how many it saved. The trades that are waiting go first, each in a batch
  /// with both its players, into the trades table, and the war kills, a row
  /// each, into the war_kills table. A batch stays in Redis, in a set
  /// of its own, until MySQL has it, so that one a flush took and didn't save,
  /// as its saver died, goes back with the unsaved players as the next flush
  /// starts. There's one saver at a time.
  absl::StatusOr<int> Flush();

  /// How many players are marked unsaved in Redis, waiting for a flush, or in
  /// a batch that isn't in MySQL yet, or both, each once.
  absl::StatusOr<int> Unsaved();

  /// The serials that more than one item in MySQL has, as only a copy of an
  /// item can have another's.
  absl::StatusOr<int> Duplicates();

 private:
  /// Puts a batch that wasn't saved back into the sets of those unsaved.
  absl::Status PutBack();

  Database& database_;
  Redis& redis_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_PLAYERS_H_
