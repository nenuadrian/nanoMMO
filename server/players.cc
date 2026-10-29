#include "server/players.h"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "content/items.h"
#include "google/protobuf/util/json_util.h"
#include "server/inventory.h"
#include "terrain/lands.h"

namespace nanommo {

namespace {

constexpr char kUnsaved[] = "players:unsaved";
// The players whose bags changed, which are in kUnsaved too.
constexpr char kItemsUnsaved[] = "items:unsaved";

std::string Key(const std::string& world, uint32_t account_id) {
  return absl::StrCat("player:", world, ":", account_id);
}

// In words, as lands are, so that the rows read well: "bag 0" to "bag 23", or
// the slot that's worn, such as "main hand".
std::string PlaceName(const protocol::Place& place) {
  return place.has_bag() ? absl::StrCat("bag ", place.bag())
                         : std::string(SlotName(place.worn()));
}

absl::StatusOr<protocol::Place> PlaceNamed(std::string_view name) {
  uint32_t slot = 0;
  if (name.starts_with("bag ") && absl::SimpleAtoi(name.substr(4), &slot)) {
    return InBag(slot);
  }
  for (int worn = items::Slot_MIN; worn <= items::Slot_MAX; ++worn) {
    if (items::Slot_IsValid(worn) &&
        SlotName(static_cast<items::Slot>(worn)) == name) {
      return Worn(static_cast<items::Slot>(worn));
    }
  }
  return absl::InternalError(
      absl::StrCat("an item saved in ", name, ", which isn't a place we know"));
}

// What one of an item has of its own, past its item and count, which have
// columns of their own, as protobuf's JSON for a stack, or nothing. A field
// that a later server adds goes in without a migration.
std::string StateOf(const items::Stack& stack) {
  items::Stack state = stack;
  state.clear_item();
  state.clear_count();
  std::string json;
  if (state.ByteSizeLong() == 0 ||
      !google::protobuf::util::MessageToJsonString(state, &json).ok()) {
    return "";
  }
  return json;
}

// A player's quests as one message, which Redis keeps the bytes of, and
// MySQL the JSON.
protocol::Quests QuestsOf(const Progress& progress) {
  protocol::Quests quests;
  quests.mutable_on()->insert(progress.quests.begin(), progress.quests.end());
  quests.mutable_done()->Add(progress.quests_done.begin(),
                             progress.quests_done.end());
  return quests;
}

// The bag from its rows: place, item, count and state. What an earlier
// server doesn't know of a later one's state, it leaves out.
absl::StatusOr<protocol::Inventory> BagOf(const Result& rows,
                                          const std::string& revision) {
  protocol::Inventory bag;
  uint32_t number = 0;
  if (!revision.empty() && !absl::SimpleAtoi(revision, &number)) {
    return absl::InternalError("a saved revision that isn't a number");
  }
  bag.set_revision(number);
  google::protobuf::util::JsonParseOptions options;
  options.ignore_unknown_fields = true;
  for (const std::vector<std::string>& row : rows.rows) {
    const absl::StatusOr<protocol::Place> place = PlaceNamed(row[0]);
    if (!place.ok()) {
      return place.status();
    }
    items::Stack stack;
    if (!row[3].empty() &&
        !google::protobuf::util::JsonStringToMessage(row[3], &stack, options)
             .ok()) {
      return absl::InternalError("a saved item whose state isn't a stack's");
    }
    uint32_t item = 0;
    uint32_t count = 0;
    if (!absl::SimpleAtoi(row[1], &item) || !absl::SimpleAtoi(row[2], &count)) {
      return absl::InternalError("a saved item that isn't numbers");
    }
    stack.set_item(item);
    stack.set_count(count);
    auto& slots = place->has_bag() ? *bag.mutable_bag() : *bag.mutable_worn();
    slots[place->has_bag() ? place->bag()
                           : static_cast<uint32_t>(place->worn())] =
        std::move(stack);
  }
  return bag;
}

}  // namespace

absl::StatusOr<Player> Players::Load(const std::string& world,
                                     uint32_t account_id) {
  const absl::StatusOr<Result> result = database_.Run(
      "SELECT accounts.username, COALESCE(players.x, 0), "
      "COALESCE(players.z, 0), COALESCE(players.experience, 0), "
      "players.health, COALESCE(players.land, 'Meadows'), "
      "players.items_revision, players.quests "
      "FROM accounts "
      "LEFT JOIN players ON players.account_id = accounts.id "
      "AND players.world = ? "
      "WHERE accounts.id = ?",
      {world, std::to_string(account_id)});
  if (!result.ok()) {
    return result.status();
  }
  if (result->rows.empty()) {
    return absl::NotFoundError(absl::StrCat("no account ", account_id));
  }
  const std::vector<std::string>& row = result->rows.front();
  Player player{.name = row[0]};
  // Where they are, their experience, health and land, and next their skills.
  std::vector<std::string> saved(row.begin() + 1, row.begin() + 6);
  // A row for each skill, which we join the way Redis keeps them.
  const absl::StatusOr<Result> skills = database_.Run(
      "SELECT skill FROM player_skills WHERE account_id = ? AND world = ?",
      {std::to_string(account_id), world});
  if (!skills.ok()) {
    return skills.status();
  }
  std::vector<std::string> known;
  for (const std::vector<std::string>& skill : skills->rows) {
    known.push_back(skill[0]);
  }
  saved.push_back(absl::StrJoin(known, ","));
  // And a row for each stack in the bag, and each item worn.
  const absl::StatusOr<Result> items = database_.Run(
      "SELECT place, item, count, state FROM player_items "
      "WHERE account_id = ? AND world = ?",
      {std::to_string(account_id), world});
  if (!items.ok()) {
    return items.status();
  }
  absl::StatusOr<protocol::Inventory> bag = BagOf(*items, row[6]);
  if (!bag.ok()) {
    return bag.status();
  }
  // And the quests, from their JSON.
  protocol::Quests quests;
  google::protobuf::util::JsonParseOptions options;
  options.ignore_unknown_fields = true;
  if (!row[7].empty() &&
      !google::protobuf::util::JsonStringToMessage(row[7], &quests, options)
           .ok()) {
    return absl::InternalError("saved quests that aren't any");
  }

  const absl::StatusOr<std::vector<std::vector<std::string>>> newer =
      redis_.Pipeline({{"HMGET", Key(world, account_id), "x", "z", "experience",
                        "health", "land", "skills", "items", "quests"}});
  if (!newer.ok()) {
    return newer.status();
  }
  const std::vector<std::string>& hash = newer->front();
  if (!hash[0].empty()) {
    saved.assign(hash.begin(), hash.begin() + 6);
  }
  // Redis has the bag from when it last changed, and a hash from a server
  // from before bags, or one written since the bag came from MySQL, has none,
  // so MySQL's is the newest.
  if (!hash[6].empty() && !bag->ParseFromString(hash[6])) {
    return absl::InternalError("a saved bag that isn't one");
  }
  // The same goes for the quests, from a server from before them.
  if (!hash[7].empty() && !quests.ParseFromString(hash[7])) {
    return absl::InternalError("saved quests that aren't any");
  }
  player.quests = {quests.on().begin(), quests.on().end()};
  player.quests_done = {quests.done().begin(), quests.done().end()};
  player.inventory = Inventory(*std::move(bag));
  if (!absl::SimpleAtof(saved[0], &player.x) ||
      !absl::SimpleAtof(saved[1], &player.z) ||
      !absl::SimpleAtoi(saved[2], &player.experience)) {
    return absl::InternalError("a saved player that isn't numbers");
  }
  if (!saved[3].empty() && !absl::SimpleAtoi(saved[3], &player.health)) {
    return absl::InternalError("a saved health that isn't a number");
  }
  player.land = LandNamed(saved[4]);
  for (const std::string_view skill :
       absl::StrSplit(saved[5], ',', absl::SkipEmpty())) {
    uint32_t id = 0;
    if (!absl::SimpleAtoi(skill, &id)) {
      return absl::InternalError("a saved skill that isn't a number");
    }
    player.skills.insert(id);
  }
  return player;
}

absl::Status Players::Save(
    const std::string& world, const std::map<uint32_t, Progress>& changed,
    const std::map<uint32_t, protocol::Inventory>& bags) {
  if (changed.empty()) {
    return absl::OkStatus();
  }
  std::vector<std::vector<std::string>> commands;
  std::vector<std::string> unsaved = {"SADD", kUnsaved};
  std::vector<std::string> items_unsaved = {"SADD", kItemsUnsaved};
  for (const auto& [id, progress] : changed) {
    std::vector<std::string>& hset =
        commands.emplace_back(std::vector<std::string>{
            "HSET",       Key(world, id),
            "world",      world,
            "account",    absl::StrCat(id),
            "x",          absl::StrCat(progress.x),
            "z",          absl::StrCat(progress.z),
            "experience", absl::StrCat(progress.experience),
            "health",     absl::StrCat(progress.health),
            "land",       kLands[progress.land].name,
            "weapon",     absl::StrCat(progress.weapon),
            "skills",     absl::StrJoin(progress.skills, ","),
            "quests",     QuestsOf(progress).SerializeAsString()});
    unsaved.push_back(Key(world, id));
    // The bag goes as its message, in protobuf's bytes, which Redis keeps as
    // they are.
    if (const auto bag = bags.find(id); bag != bags.end()) {
      hset.insert(hset.end(), {"items", bag->second.SerializeAsString()});
      items_unsaved.push_back(Key(world, id));
    }
  }
  commands.push_back(unsaved);
  if (items_unsaved.size() > 2) {
    commands.push_back(items_unsaved);
  }
  return redis_.Pipeline(commands).status();
}

absl::StatusOr<int> Players::Flush() {
  // As many as are unsaved now: whoever changes meanwhile waits for next time.
  const absl::StatusOr<std::string> count = redis_.Command({"SCARD", kUnsaved});
  int left = 0;
  if (!count.ok() || !absl::SimpleAtoi(*count, &left)) {
    return absl::UnavailableError("couldn't count the unsaved players");
  }
  int saved = 0;
  while (left > 0) {
    // SPOP takes them out of the set, so one who changes after this is marked
    // unsaved again, and goes in a later batch.
    const absl::StatusOr<std::vector<std::vector<std::string>>> popped =
        redis_.Pipeline(
            {{"SPOP", kUnsaved, absl::StrCat(std::min(left, kBatch))}});
    if (!popped.ok()) {
      return popped.status();
    }
    const std::vector<std::string>& keys = popped->front();
    if (keys.empty()) {
      break;
    }
    left -= static_cast<int>(keys.size());

    // Each player's mark for their bag comes off before their hash is read,
    // so that a bag that changes in between is marked again, for next time.
    std::vector<std::vector<std::string>> reads;
    reads.reserve(2 * keys.size());
    for (const std::string& key : keys) {
      reads.push_back({"SREM", kItemsUnsaved, key});
    }
    for (const std::string& key : keys) {
      // Before worlds, the set held account IDs, for hashes with no world.
      if (key.starts_with("player:")) {
        reads.push_back({"HMGET", key, "world", "account", "x", "z",
                         "experience", "health", "land", "weapon", "skills",
                         "items", "quests"});
      } else {
        reads.push_back({"HMGET", absl::StrCat("player:", key), "x", "z",
                         "experience", "health", "land", "weapon"});
      }
    }
    const absl::StatusOr<std::vector<std::vector<std::string>>> replies =
        redis_.Pipeline(reads);
    if (!replies.ok()) {
      return replies.status();
    }

    // IGNORE skips a player whose account is gone, with a warning, so that one
    // row MySQL refuses doesn't hold back the batch on each flush.
    std::string sql =
        "INSERT IGNORE INTO players (world, account_id, x, z, experience, "
        "health, land, weapon, items_revision, quests) VALUES ";
    std::vector<std::string> values;
    int rows = 0;
    std::string skills_sql =
        "INSERT IGNORE INTO player_skills (account_id, world, skill) VALUES ";
    std::vector<std::string> skill_values;
    // The bags that changed: their old rows go, and their new ones go in.
    std::vector<std::string> bag_keys;
    std::string gone_sql =
        "DELETE FROM player_items WHERE (account_id, world) IN (";
    std::vector<std::string> gone_values;
    // Six values a row.
    std::vector<std::string> item_values;
    for (std::size_t i = 0; i < keys.size(); ++i) {
      const bool bag_changed = (*replies)[i] == std::vector<std::string>{"1"};
      const std::vector<std::string>& hash = (*replies)[keys.size() + i];
      // A player whose hash is gone has nothing to save.
      if (hash[0].empty()) {
        continue;
      }
      // A hash saved before we kept health or weapons has none, which goes in
      // as NULL, and one saved before lands is in the Meadows. A bag that
      // hasn't changed keeps the revision MySQL has.
      absl::StrAppend(&sql, rows == 0 ? "" : ", ",
                      "(?, ?, ?, ?, ?, NULLIF(?, ''), "
                      "COALESCE(NULLIF(?, ''), 'Meadows'), NULLIF(?, ''), "
                      "NULLIF(?, ''), NULLIF(?, ''))");
      ++rows;
      if (!keys[i].starts_with("player:")) {
        values.push_back("Ember");
        values.push_back(keys[i]);
        values.insert(values.end(), hash.begin(), hash.end());
        values.insert(values.end(), {"", ""});
        continue;
      }
      const std::string& world = hash[0];
      const std::string& account = hash[1];
      values.insert(values.end(), hash.begin(), hash.begin() + 8);
      protocol::Inventory bag;
      if (!bag_changed || hash[9].empty() || !bag.ParseFromString(hash[9])) {
        values.push_back("");
      } else {
        values.push_back(absl::StrCat(bag.revision()));
        bag_keys.push_back(keys[i]);
        absl::StrAppend(&gone_sql, gone_values.empty() ? "" : ", ", "(?, ?)");
        gone_values.insert(gone_values.end(), {account, world});
        const auto add = [&](const protocol::Place& place,
                             const items::Stack& stack) {
          item_values.insert(
              item_values.end(),
              {account, world, PlaceName(place), absl::StrCat(stack.item()),
               absl::StrCat(stack.count()), StateOf(stack)});
        };
        for (const auto& [slot, stack] : bag.bag()) {
          add(InBag(slot), stack);
        }
        for (const auto& [slot, stack] : bag.worn()) {
          add(Worn(static_cast<items::Slot>(slot)), stack);
        }
      }
      // The quests go with the row, as JSON, and a hash with none, from before
      // quests or of a player on none, leaves MySQL's.
      protocol::Quests quests;
      std::string json;
      if (!hash[10].empty() && quests.ParseFromString(hash[10])) {
        google::protobuf::util::MessageToJsonString(quests, &json)
            .IgnoreError();
      }
      values.push_back(json);
      // The skills go into a table of their own, a row each.
      for (const std::string_view skill :
           absl::StrSplit(hash[8], ',', absl::SkipEmpty())) {
        absl::StrAppend(&skills_sql, skill_values.empty() ? "" : ", ",
                        "(?, ?, ?)");
        skill_values.insert(skill_values.end(),
                            {account, world, std::string(skill)});
      }
    }
    if (rows == 0) {
      continue;
    }
    absl::StrAppend(&sql,
                    " AS saved ON DUPLICATE KEY UPDATE x = saved.x, "
                    "z = saved.z, experience = saved.experience, "
                    "health = saved.health, land = saved.land, "
                    "weapon = saved.weapon, items_revision = "
                    "COALESCE(saved.items_revision, players.items_revision), "
                    "quests = COALESCE(saved.quests, players.quests)");
    absl::StrAppend(&gone_sql, ")");
    // A bag's rows can't go without the rows that take their place, or a
    // failure between the two would empty it, so the batch is a transaction.
    const absl::Status written = database_.Transaction([&] {
      absl::StatusOr<Result> result = database_.Run(sql, values);
      // A skill once learned stays learned, so each flush inserts each skill
      // a player knows, and IGNORE skips the ones MySQL has already, and those
      // of a player whose row it refused.
      if (result.ok() && !skill_values.empty()) {
        result = database_.Run(skills_sql, skill_values);
      }
      if (result.ok() && !gone_values.empty()) {
        result = database_.Run(gone_sql, gone_values);
      }
      // A statement takes up to 65,535 values, and the rows of 500 full bags
      // have more, so they go in statements of kItemRows rows.
      constexpr std::size_t kRowValues = 6;
      for (std::size_t first = 0; result.ok() && first < item_values.size();
           first += kItemRows * kRowValues) {
        const std::size_t last =
            std::min(item_values.size(), first + kItemRows * kRowValues);
        std::string items_sql =
            "INSERT IGNORE INTO player_items (account_id, world, place, item, "
            "count, state) VALUES ";
        for (std::size_t row = first; row < last; row += kRowValues) {
          absl::StrAppend(&items_sql, row == first ? "" : ", ",
                          "(?, ?, ?, ?, ?, NULLIF(?, ''))");
        }
        result = database_.Run(items_sql, {item_values.begin() + first,
                                           item_values.begin() + last});
      }
      return result.status();
    });
    if (!written.ok()) {
      // Back into the sets, so that the next flush tries them again.
      std::vector<std::string> again = {"SADD", kUnsaved};
      again.insert(again.end(), keys.begin(), keys.end());
      std::vector<std::vector<std::string>> marks = {again};
      if (!bag_keys.empty()) {
        std::vector<std::string>& bags_again =
            marks.emplace_back(std::vector<std::string>{"SADD", kItemsUnsaved});
        bags_again.insert(bags_again.end(), bag_keys.begin(), bag_keys.end());
      }
      redis_.Pipeline(marks).IgnoreError();
      return written;
    }
    saved += rows;
  }
  return saved;
}

}  // namespace nanommo
