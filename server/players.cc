#include "server/players.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "content/items.h"
#include "google/protobuf/util/json_util.h"
#include "server/inventory.h"
#include "server/tracing.h"
#include "terrain/lands.h"

namespace nanommo {

namespace {

constexpr char kUnsaved[] = "players:unsaved";
// The players whose bags changed, which are in kUnsaved too.
constexpr char kItemsUnsaved[] = "items:unsaved";
// The players, and the bags, in the batch the saver's taking to MySQL. They
// stay here until MySQL has them, so that a saver that dies with a batch, or
// whose MySQL fails, leaves it for the next flush, rather than losing it.
constexpr char kSaving[] = "players:saving";
constexpr char kItemsSaving[] = "items:saving";
// The trades that aren't in MySQL yet, oldest first.
constexpr char kTrades[] = "trades";
// The war kills that aren't on their wars' scores in MySQL yet, oldest first.
constexpr char kWarKills[] = "war_kills";

std::string Key(const std::string& world, uint32_t id) {
  return absl::StrCat("player:", world, ":", id);
}

// The lease on a player, which names the instance of the world that plays
// them.
std::string LeaseKey(const std::string& world, uint32_t id) {
  return absl::StrCat("playing:", world, ":", id);
}

// The world's counter, which each lease takes its fencing number from.
std::string FencingKey(const std::string& world) {
  return absl::StrCat("fencing:", world);
}

// The players whose leases these are, by their keys.
std::set<uint32_t> Leased(const std::vector<std::string>& keys) {
  std::set<uint32_t> players;
  for (const std::string& key : keys) {
    uint32_t id = 0;
    if (absl::SimpleAtoi(std::string_view(key).substr(key.rfind(':') + 1),
                         &id)) {
      players.insert(id);
    }
  }
  return players;
}

// Each script runs in Redis as one, with nothing else in between, so a lease
// can't change between reading it and writing it.
//
// Takes the lease in KEYS[1] for the instance in ARGV[1], for ARGV[2]
// milliseconds, with the next number from the counter in KEYS[2], unless
// another instance has it, so that a hello it turns away uses up no number.
// One handed to this instance is its already, and runs for as long again.
// Returns the lease as it is then.
constexpr char kTake[] = R"lua(
local lease = redis.call('GET', KEYS[1])
if not lease then
  lease = ARGV[1] .. ' ' .. redis.call('INCR', KEYS[2])
  redis.call('SET', KEYS[1], lease, 'PX', ARGV[2])
elseif string.sub(lease, 1, #ARGV[1] + 1) == ARGV[1] .. ' ' then
  redis.call('PEXPIRE', KEYS[1], ARGV[2])
end
return lease
)lua";

// Each lease in KEYS that's still the same in ARGV, past the first, runs for
// another ARGV[1] milliseconds. Returns those that aren't.
constexpr char kRenew[] = R"lua(
local lost = {}
for i, key in ipairs(KEYS) do
  if redis.call('GET', key) == ARGV[i + 1] then
    redis.call('PEXPIRE', key, ARGV[1])
  else
    lost[#lost + 1] = key
  end
end
return lost
)lua";

// Deletes the lease in KEYS[1] if it's still ARGV[1].
constexpr char kRelease[] = R"lua(
if redis.call('GET', KEYS[1]) == ARGV[1] then
  return redis.call('DEL', KEYS[1])
end
return 0
)lua";

// Hands each lease in KEYS, past the counter in KEYS[1], that's still the
// same in ARGV, past the first two, to the instance in ARGV[1], with the next
// number from the counter, for ARGV[2] milliseconds. Returns those it handed.
constexpr char kPass[] = R"lua(
local passed = {}
for i = 2, #KEYS do
  if redis.call('GET', KEYS[i]) == ARGV[i + 1] then
    local lease = ARGV[1] .. ' ' .. redis.call('INCR', KEYS[1])
    redis.call('SET', KEYS[i], lease, 'PX', ARGV[2])
    passed[#passed + 1] = KEYS[i]
  end
end
return passed
)lua";

// Takes up to ARGV[1] players out of the set in KEYS[1] and into the one in
// KEYS[2], at once, as SPOP and SADD would one after the other, and returns
// them.
constexpr char kTakeBatch[] = R"lua(
local taken = redis.call('SPOP', KEYS[1], ARGV[1])
if #taken > 0 then
  redis.call('SADD', KEYS[2], unpack(taken))
end
return taken
)lua";

// How many members the sets in KEYS[1] and KEYS[2] have between them, each
// once, however many of the two it's in.
constexpr char kCountUnion[] = R"lua(
return #redis.call('SUNION', KEYS[1], KEYS[2])
)lua";

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

// What one side gave in a trade, as protobuf's JSON, which MySQL keeps.
std::string GaveOf(protocol::TradeSide side) {
  side.clear_ready();
  std::string json;
  google::protobuf::util::MessageToJsonString(side, &json).IgnoreError();
  return json;
}

// The value of a stream entry's field, or nothing if it hasn't the field.
std::string FieldOf(const Redis::Entry& entry, std::string_view name) {
  for (std::size_t i = 0; i + 1 < entry.fields.size(); i += 2) {
    if (entry.fields[i] == name) {
      return entry.fields[i + 1];
    }
  }
  return "";
}

// The span a batch's trades go on with their traces in, from when they were
// read off the stream: under the oldest's, as a span has one parent, and
// linked to the rest. None for a batch without traced trades.
Span StartSavingTrades(const std::vector<Redis::Entry>& trades,
                       absl::Time read) {
  std::vector<SpanContext> published;
  for (const Redis::Entry& trade : trades) {
    if (const absl::StatusOr<SpanContext> context =
            ParseTraceparent(FieldOf(trade, "traceparent"));
        context.ok()) {
      published.push_back(*context);
    }
  }
  if (published.empty()) {
    return Span();
  }
  Span span =
      StartSpan("save trades", SpanKind::kConsumer, published.front(), read);
  for (std::size_t i = 1; i < published.size(); ++i) {
    span.AddLink(published[i]);
  }
  span.SetAttribute("messaging.system", "redis");
  span.SetAttribute("messaging.destination.name", kTrades);
  span.SetAttribute("messaging.batch.message_count",
                    static_cast<int64_t>(trades.size()));
  return span;
}

// A player's quests as one message, which Redis keeps the bytes of, and
// MySQL the JSON.
protocol::Quests QuestsOf(const Progress& progress) {
  protocol::Quests quests;
  quests.mutable_on()->insert(progress.quests.begin(), progress.quests.end());
  quests.mutable_done()->Add(progress.quests_done.begin(),
                             progress.quests_done.end());
  quests.set_revision(progress.quests_revision);
  return quests;
}

// The bag from its rows: place, item, count and state, with its revision
// and gold from the player's row. What an earlier server doesn't know of a
// later one's state, it leaves out.
absl::StatusOr<protocol::Inventory> BagOf(const Result& rows,
                                          const std::string& revision,
                                          const std::string& gold) {
  protocol::Inventory bag;
  uint32_t number = 0;
  if (!revision.empty() && !absl::SimpleAtoi(revision, &number)) {
    return absl::InternalError("a saved revision that isn't a number");
  }
  bag.set_revision(number);
  uint32_t coins = 0;
  if (!gold.empty() && !absl::SimpleAtoi(gold, &coins)) {
    return absl::InternalError("a saved gold that isn't a number");
  }
  bag.set_gold(coins);
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

absl::StatusOr<Player> Players::Load(const std::string& world, uint32_t id) {
  Span span = StartChildSpan("load player", SpanKind::kInternal);
  // A character in another world is no player of this one's.
  const absl::StatusOr<Result> result = database_.Run(
      "SELECT name, x, z, experience, health, land, items_revision, quests, "
      "gold, build, skin, hair_color, hair, shirt, faction FROM players "
      "WHERE id = ? AND world = ?",
      {std::to_string(id), world});
  if (!result.ok()) {
    return result.status();
  }
  if (result->rows.empty()) {
    return absl::NotFoundError(
        absl::StrCat("no character ", id, " in ", world));
  }
  const std::vector<std::string>& row = result->rows.front();
  Player player{.name = row[0]};
  // How they chose to look, which never changes, so it's only ever in MySQL.
  // A character from before there were looks to choose has none.
  if (!row[9].empty()) {
    protocol::Looks looks;
    uint32_t build = 0;
    uint32_t skin = 0;
    uint32_t hair_color = 0;
    uint32_t hair = 0;
    if (!absl::SimpleAtoi(row[9], &build) ||
        !absl::SimpleAtoi(row[10], &skin) ||
        !absl::SimpleAtoi(row[11], &hair_color) ||
        !absl::SimpleAtoi(row[12], &hair)) {
      return absl::InternalError("looks that aren't numbers");
    }
    looks.set_build(build);
    looks.set_skin(skin);
    looks.set_hair_color(hair_color);
    looks.set_hair(hair);
    // And the colour of their clothes, unless they're from before there was
    // one to choose.
    uint32_t shirt = 0;
    if (!row[13].empty()) {
      if (!absl::SimpleAtoi(row[13], &shirt)) {
        return absl::InternalError("looks that aren't numbers");
      }
      looks.set_shirt(shirt);
    }
    player.looks = looks;
  }
  // Where they are, their experience, health and land, and next their skills.
  std::vector<std::string> saved(row.begin() + 1, row.begin() + 6);
  // A row for each skill, which we join the way Redis keeps them.
  const absl::StatusOr<Result> skills = database_.Run(
      "SELECT skill FROM player_skills WHERE player = ?", {std::to_string(id)});
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
      "SELECT place, item, count, state FROM player_items WHERE player = ?",
      {std::to_string(id)});
  if (!items.ok()) {
    return items.status();
  }
  absl::StatusOr<protocol::Inventory> bag = BagOf(*items, row[6], row[8]);
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
      redis_.Pipeline(
          {{"HMGET", Key(world, id), "x", "z", "experience", "health", "land",
            "skills", "items", "quests", "faction"}});
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
  player.quests_revision = quests.revision();
  // Their faction is for good: MySQL's, once the saver's taken it there, or
  // Redis's until then. None is empty in both.
  const std::string& faction = !row[14].empty() ? row[14] : hash[8];
  if (!faction.empty() && !absl::SimpleAtoi(faction, &player.faction)) {
    return absl::InternalError("a saved faction that isn't a number");
  }
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

absl::StatusOr<std::string> Players::Lease(const std::string& world,
                                           uint32_t id,
                                           const std::string& instance) {
  Span span = StartChildSpan("lease player", SpanKind::kInternal);
  absl::StatusOr<std::string> lease = redis_.Command(
      {"EVAL", kTake, "2", LeaseKey(world, id), FencingKey(world), instance,
       absl::StrCat(kLease.count())});
  if (!lease.ok()) {
    return lease.status();
  }
  // The instance that has it comes before its number.
  const std::string_view holder =
      std::string_view(*lease).substr(0, lease->rfind(' '));
  if (holder != instance) {
    return absl::FailedPreconditionError(
        absl::StrCat("they're playing on ", holder));
  }
  return lease;
}

absl::StatusOr<std::set<uint32_t>> Players::Renew(
    const std::string& world, const std::map<uint32_t, std::string>& leases) {
  if (leases.empty()) {
    return std::set<uint32_t>();
  }
  std::vector<std::string> eval = {"EVAL", kRenew, absl::StrCat(leases.size())};
  for (const auto& [id, lease] : leases) {
    eval.push_back(LeaseKey(world, id));
  }
  eval.push_back(absl::StrCat(kLease.count()));
  for (const auto& [id, lease] : leases) {
    eval.push_back(lease);
  }
  const absl::StatusOr<std::vector<std::vector<std::string>>> lost =
      redis_.Pipeline({eval});
  if (!lost.ok()) {
    return lost.status();
  }
  return Leased(lost->front());
}

absl::Status Players::Release(const std::string& world, uint32_t id,
                              const std::string& lease) {
  return redis_.Command({"EVAL", kRelease, "1", LeaseKey(world, id), lease})
      .status();
}

absl::StatusOr<std::set<uint32_t>> Players::Pass(
    const std::string& world, const std::map<uint32_t, std::string>& leases,
    const std::string& next) {
  Span span = StartChildSpan("pass leases", SpanKind::kInternal);
  std::vector<std::string> eval = {
      "EVAL", kPass, absl::StrCat(leases.size() + 1), FencingKey(world)};
  for (const auto& [id, lease] : leases) {
    eval.push_back(LeaseKey(world, id));
  }
  eval.insert(eval.end(), {next, absl::StrCat(kLease.count())});
  for (const auto& [id, lease] : leases) {
    eval.push_back(lease);
  }
  const absl::StatusOr<std::vector<std::vector<std::string>>> passed =
      redis_.Pipeline({eval});
  if (!passed.ok()) {
    return passed.status();
  }
  return Leased(passed->front());
}

absl::StatusOr<std::set<uint32_t>> Players::Save(
    const std::string& world, const std::map<uint32_t, Progress>& changed,
    const std::map<uint32_t, protocol::Inventory>& bags,
    const std::map<uint32_t, std::string>& leases,
    const std::optional<Traded>& traded, const std::vector<WarKill>& kills) {
  if (changed.empty() && !traded && kills.empty()) {
    return std::set<uint32_t>();
  }
  Span span = StartChildSpan("save players", SpanKind::kInternal);
  std::vector<std::vector<std::string>> commands;
  std::vector<std::string> unsaved = {"SADD", kUnsaved};
  std::vector<std::string> items_unsaved = {"SADD", kItemsUnsaved};
  for (const auto& [id, progress] : changed) {
    // A player who's sworn to no faction has none, as in a hash from before
    // factions.
    const std::string faction =
        progress.faction != 0 ? absl::StrCat(progress.faction) : "";
    std::vector<std::string>& hset =
        commands.emplace_back(std::vector<std::string>{
            "HSET",       Key(world, id),
            "world",      world,
            "player",     absl::StrCat(id),
            "x",          absl::StrCat(progress.x),
            "z",          absl::StrCat(progress.z),
            "experience", absl::StrCat(progress.experience),
            "health",     absl::StrCat(progress.health),
            "land",       kLands[progress.land].name,
            "weapon",     absl::StrCat(progress.weapon),
            "skills",     absl::StrJoin(progress.skills, ","),
            "quests",     QuestsOf(progress).SerializeAsString(),
            "faction",    faction});
    unsaved.push_back(Key(world, id));
    // The bag goes as its message, in protobuf's bytes, which Redis keeps as
    // they are.
    if (const auto bag = bags.find(id); bag != bags.end()) {
      hset.insert(hset.end(), {"items", bag->second.SerializeAsString()});
      items_unsaved.push_back(Key(world, id));
    }
  }
  if (unsaved.size() > 2) {
    commands.push_back(unsaved);
  }
  if (items_unsaved.size() > 2) {
    commands.push_back(items_unsaved);
  }
  // The trade's record goes with its bags, so the saver takes them to MySQL
  // together. It's a message to the saver, through the stream, with the
  // traceparent of this span, which the saver's span for it goes under.
  const Span publishing =
      traded ? StartChildSpan("publish trade", SpanKind::kProducer) : Span();
  if (traded) {
    std::vector<std::string>& added =
        commands.emplace_back(std::vector<std::string>{
            "XADD", kTrades, "*", "id", traded->id, "world", world, "player",
            absl::StrCat(traded->player), "player_gave",
            GaveOf(traded->player_gave), "partner",
            absl::StrCat(traded->partner), "partner_gave",
            GaveOf(traded->partner_gave)});
    if (publishing.recording()) {
      added.insert(added.end(),
                   {"traceparent", Traceparent(publishing.context())});
    }
  }
  // Each war kill goes to the saver in a stream of its own, with the players
  // it changed, for its war's score.
  for (const WarKill& kill : kills) {
    commands.push_back(
        {"XADD", kWarKills, "*", "id", kill.id, "war", absl::StrCat(kill.war),
         "world", world, "killer", absl::StrCat(kill.killer), "victim",
         absl::StrCat(kill.victim), "guild", absl::StrCat(kill.guild)});
  }
  // Another instance that's taken a player has saved them since, and its
  // saves are the ones to keep.
  std::map<std::string, std::string> holding;
  for (const auto& [id, lease] : leases) {
    holding[LeaseKey(world, id)] = lease;
  }
  const absl::StatusOr<std::vector<std::string>> lost =
      redis_.TransactionIf(holding, commands);
  if (!lost.ok()) {
    return lost.status();
  }
  return Leased(*lost);
}

absl::Status Players::PutBack() {
  // SUNIONSTORE takes the set it stores into as one of its sources, so a
  // player marked unsaved again since stays so, once.
  return redis_.Transaction(
      {{"SUNIONSTORE", kUnsaved, kUnsaved, kSaving},
       {"DEL", kSaving},
       {"SUNIONSTORE", kItemsUnsaved, kItemsUnsaved, kItemsSaving},
       {"DEL", kItemsSaving}});
}

absl::StatusOr<int> Players::Flush() {
  // A batch that a flush before took and didn't save, as the saver died, or
  // MySQL failed, goes back with the unsaved players first.
  if (const absl::Status back = PutBack(); !back.ok()) {
    return back;
  }
  // As many as are unsaved now, and as many trades and war kills: whoever
  // changes meanwhile waits for next time.
  const absl::StatusOr<std::vector<std::vector<std::string>>> counts =
      redis_.Pipeline(
          {{"SCARD", kUnsaved}, {"XLEN", kTrades}, {"XLEN", kWarKills}});
  int left = 0;
  int trades_left = 0;
  int kills_left = 0;
  if (!counts.ok() || !absl::SimpleAtoi(counts->at(0).at(0), &left) ||
      !absl::SimpleAtoi(counts->at(1).at(0), &trades_left) ||
      !absl::SimpleAtoi(counts->at(2).at(0), &kills_left)) {
    return absl::UnavailableError("couldn't count the unsaved players");
  }
  int saved = 0;
  while (left > 0 || trades_left > 0 || kills_left > 0) {
    // The oldest trades go first, each with both its players, so that MySQL
    // has a trade's record and both bags from after it, or none of it.
    std::vector<Redis::Entry> trades;
    const absl::Time read = absl::Now();
    if (trades_left > 0) {
      absl::StatusOr<std::vector<Redis::Entry>> oldest = redis_.Read(
          {"XREAD", "COUNT", absl::StrCat(std::min(trades_left, kBatch / 2)),
           "STREAMS", kTrades, "0-0"});
      if (!oldest.ok()) {
        return oldest.status();
      }
      trades = *std::move(oldest);
      trades_left =
          trades.empty() ? 0 : trades_left - static_cast<int>(trades.size());
    }
    // And the oldest war kills, which go onto their wars' scores with it.
    std::vector<Redis::Entry> kills;
    if (kills_left > 0) {
      absl::StatusOr<std::vector<Redis::Entry>> oldest = redis_.Read(
          {"XREAD", "COUNT", absl::StrCat(std::min(kills_left, kBatch)),
           "STREAMS", kWarKills, "0-0"});
      if (!oldest.ok()) {
        return oldest.status();
      }
      kills = *std::move(oldest);
      kills_left =
          kills.empty() ? 0 : kills_left - static_cast<int>(kills.size());
    }
    // The batch's statements in MySQL go under it.
    const Span saving = StartSavingTrades(trades, read);
    std::vector<std::string> keys;
    const auto take = [&keys](const std::string& key) {
      if (std::ranges::find(keys, key) == keys.end()) {
        keys.push_back(key);
      }
    };
    for (const Redis::Entry& trade : trades) {
      for (const char* side : {"player", "partner"}) {
        uint32_t id = 0;
        if (absl::SimpleAtoi(FieldOf(trade, side), &id)) {
          take(Key(FieldOf(trade, "world"), id));
        }
      }
    }
    // They're in this batch, so they go from the set into the batch's.
    if (!keys.empty()) {
      std::vector<std::vector<std::string>> taken;
      taken.reserve(keys.size());
      for (const std::string& key : keys) {
        taken.push_back({"SMOVE", kUnsaved, kSaving, key});
      }
      const absl::StatusOr<std::vector<std::vector<std::string>>> moved =
          redis_.Pipeline(taken);
      if (!moved.ok()) {
        return moved.status();
      }
    }
    // The rest come out of the set and into the batch's together, so one who
    // changes after this is marked unsaved again, and goes in a later batch.
    const int room = std::min(left, kBatch - static_cast<int>(keys.size()));
    if (room > 0) {
      const absl::StatusOr<std::vector<std::vector<std::string>>> popped =
          redis_.Pipeline({{"EVAL", kTakeBatch, "2", kUnsaved, kSaving,
                            absl::StrCat(room)}});
      if (!popped.ok()) {
        return popped.status();
      }
      for (const std::string& key : popped->front()) {
        take(key);
      }
      left = popped->front().empty()
                 ? 0
                 : left - static_cast<int>(popped->front().size());
    }
    if (keys.empty() && kills.empty()) {
      break;
    }

    // Each player's mark for their bag goes into the batch's before their
    // hash is read, so that a bag that changes in between is marked again,
    // for next time.
    std::vector<std::vector<std::string>> reads;
    reads.reserve(2 * keys.size());
    for (const std::string& key : keys) {
      reads.push_back({"SMOVE", kItemsUnsaved, kItemsSaving, key});
    }
    for (const std::string& key : keys) {
      reads.push_back({"HMGET", key, "world", "player", "x", "z", "experience",
                       "health", "land", "weapon", "skills", "items", "quests",
                       "faction"});
    }
    const absl::StatusOr<std::vector<std::vector<std::string>>> replies =
        redis_.Pipeline(reads);
    if (!replies.ok()) {
      return replies.status();
    }

    // The characters' rows are there already, from the login server, so a
    // batch updates them, as a table of its rows joined to theirs: a character
    // deleted since leaves nothing to join, and its row stays gone.
    std::string sql = "UPDATE players JOIN (VALUES ";
    std::vector<std::string> values;
    int rows = 0;
    // IGNORE skips the skills and the bags of a character that's gone, with a
    // warning, so that one row MySQL refuses doesn't hold back the batch.
    std::string skills_sql =
        "INSERT IGNORE INTO player_skills (player, skill) VALUES ";
    std::vector<std::string> skill_values;
    // The bags that changed: their old rows go, and their new ones go in.
    std::string gone_sql = "DELETE FROM player_items WHERE player IN (";
    std::vector<std::string> gone_values;
    // Five values a row.
    std::vector<std::string> item_values;
    for (std::size_t i = 0; i < keys.size(); ++i) {
      const bool bag_changed = (*replies)[i] == std::vector<std::string>{"1"};
      const std::vector<std::string>& hash = (*replies)[keys.size() + i];
      // A player whose hash is gone has nothing to save, and one from before
      // characters, saved by its account, has nothing to say which character
      // it was: the saver flushed those before the characters came.
      if (hash[0].empty() || hash[1].empty()) {
        continue;
      }
      // A hash saved before we kept health or weapons has none, which goes in
      // as NULL, and one saved before lands is in the Meadows. A bag that
      // hasn't changed keeps the revision and the gold MySQL has.
      absl::StrAppend(&sql, rows == 0 ? "" : ", ",
                      "ROW(?, ?, ?, ?, ?, NULLIF(?, ''), "
                      "COALESCE(NULLIF(?, ''), 'Meadows'), NULLIF(?, ''), "
                      "NULLIF(?, ''), NULLIF(?, ''), NULLIF(?, ''), "
                      "NULLIF(?, ''))");
      ++rows;
      const std::string& world = hash[0];
      const std::string& player = hash[1];
      values.insert(values.end(), {player, world});
      values.insert(values.end(), hash.begin() + 2, hash.begin() + 8);
      protocol::Inventory bag;
      if (!bag_changed || hash[9].empty() || !bag.ParseFromString(hash[9])) {
        values.insert(values.end(), {"", ""});
      } else {
        values.push_back(absl::StrCat(bag.revision()));
        values.push_back(absl::StrCat(bag.gold()));
        absl::StrAppend(&gone_sql, gone_values.empty() ? "" : ", ", "?");
        gone_values.push_back(player);
        const auto add = [&](const protocol::Place& place,
                             const items::Stack& stack) {
          item_values.insert(
              item_values.end(),
              {player, PlaceName(place), absl::StrCat(stack.item()),
               absl::StrCat(stack.count()), StateOf(stack)});
        };
        for (const auto& [slot, stack] : bag.bag()) {
          add(InBag(slot), stack);
        }
        for (const auto& [slot, stack] : bag.worn()) {
          add(Worn(static_cast<items::Slot>(slot)), stack);
        }
      }
      // The quests go with the row, as JSON, and a hash without them, from
      // before quests, leaves MySQL's. Once they've changed, they have their
      // revision, so a player who's given up all they were on saves as that.
      protocol::Quests quests;
      std::string json;
      if (!hash[10].empty() && quests.ParseFromString(hash[10])) {
        google::protobuf::util::MessageToJsonString(quests, &json)
            .IgnoreError();
      }
      values.push_back(json);
      // And their faction, if they've sworn to one.
      values.push_back(hash[11]);
      // The skills go into a table of their own, a row each.
      for (const std::string_view skill :
           absl::StrSplit(hash[8], ',', absl::SkipEmpty())) {
        absl::StrAppend(&skills_sql, skill_values.empty() ? "" : ", ",
                        "(?, ?)");
        skill_values.insert(skill_values.end(), {player, std::string(skill)});
      }
    }
    // Each trade's record, from the millisecond Redis took it, by its entry's
    // id. IGNORE skips one that's in MySQL already, from a batch that MySQL
    // committed without the saver hearing so, which it tries again.
    std::string trades_sql =
        "INSERT IGNORE INTO trades (id, world, player, player_gave, partner, "
        "partner_gave, traded_at) VALUES ";
    std::vector<std::string> trade_values;
    std::vector<std::string> recorded = {"XDEL", kTrades};
    for (const Redis::Entry& trade : trades) {
      absl::StrAppend(&trades_sql, trade_values.empty() ? "" : ", ",
                      "(?, ?, ?, ?, ?, ?, FROM_UNIXTIME(? / 1000))");
      for (const char* field : {"id", "world", "player", "player_gave",
                                "partner", "partner_gave"}) {
        trade_values.push_back(FieldOf(trade, field));
      }
      trade_values.push_back(trade.id.substr(0, trade.id.find('-')));
      recorded.push_back(trade.id);
    }
    // Each war kill is a row of its own, by the id the world server gave it,
    // from the millisecond Redis took it, as a trade's record is. IGNORE
    // skips one that's in MySQL already, from a batch MySQL committed that
    // the saver reads again, and one whose war has gone since. A kill from a
    // server before 0.92.0, during a deploy, has no id, and goes by its
    // entry's.
    std::string kills_sql =
        "INSERT IGNORE INTO war_kills (id, war, world, killer, victim, guild, "
        "killed_at) VALUES ";
    std::vector<std::string> kill_values;
    std::vector<std::string> counted = {"XDEL", kWarKills};
    for (const Redis::Entry& kill : kills) {
      absl::StrAppend(&kills_sql, kill_values.empty() ? "" : ", ",
                      "(COALESCE(NULLIF(?, ''), ?), ?, ?, ?, ?, ?, "
                      "FROM_UNIXTIME(? / 1000))");
      kill_values.push_back(FieldOf(kill, "id"));
      kill_values.push_back(kill.id);
      for (const char* field : {"war", "world", "killer", "victim", "guild"}) {
        kill_values.push_back(FieldOf(kill, field));
      }
      kill_values.push_back(kill.id.substr(0, kill.id.find('-')));
      counted.push_back(kill.id);
    }
    if (rows == 0 && trades.empty() && kills.empty()) {
      redis_.Command({"DEL", kSaving, kItemsSaving}).IgnoreError();
      continue;
    }
    absl::StrAppend(
        &sql,
        ") AS saved (id, world, x, z, experience, health, land, weapon, "
        "items_revision, gold, quests, faction) "
        "ON players.id = saved.id AND players.world = saved.world "
        "SET players.x = saved.x, players.z = saved.z, "
        "players.experience = saved.experience, players.health = saved.health, "
        "players.land = saved.land, players.weapon = saved.weapon, "
        "players.items_revision = "
        "COALESCE(saved.items_revision, players.items_revision), "
        "players.gold = COALESCE(saved.gold, players.gold), "
        "players.quests = COALESCE(saved.quests, players.quests), "
        // A faction is for good, so the saver writes one only where there's
        // none, and nothing switches it, nor does a hash with none take it.
        "players.faction = COALESCE(players.faction, saved.faction)");
    absl::StrAppend(&gone_sql, ")");
    // A bag's rows can't go without the rows that take their place, or a
    // failure between the two would empty it, so the batch is a transaction.
    const absl::Status written = database_.Transaction([&] {
      absl::StatusOr<Result> result =
          rows > 0 ? database_.Run(sql, values) : Result();
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
      constexpr std::size_t kRowValues = 5;
      for (std::size_t first = 0; result.ok() && first < item_values.size();
           first += kItemRows * kRowValues) {
        const std::size_t last =
            std::min(item_values.size(), first + kItemRows * kRowValues);
        std::string items_sql =
            "INSERT IGNORE INTO player_items (player, place, item, "
            "count, state) VALUES ";
        for (std::size_t row = first; row < last; row += kRowValues) {
          absl::StrAppend(&items_sql, row == first ? "" : ", ",
                          "(?, ?, ?, ?, NULLIF(?, ''))");
        }
        result = database_.Run(items_sql, {item_values.begin() + first,
                                           item_values.begin() + last});
      }
      if (result.ok() && !trade_values.empty()) {
        result = database_.Run(trades_sql, trade_values);
      }
      if (result.ok() && !kill_values.empty()) {
        result = database_.Run(kills_sql, kill_values);
      }
      return result.status();
    });
    if (!written.ok()) {
      // Back into the sets, so that the next flush tries them again, or, if
      // that fails too, as the next flush starts.
      PutBack().IgnoreError();
      return written;
    }
    // MySQL has them, so the batch's set goes. Should this fail, the next
    // flush saves them again, as it does the trades, which IGNORE skips.
    if (!trades.empty()) {
      redis_.Pipeline({recorded, {"DEL", kSaving, kItemsSaving}}).IgnoreError();
    } else {
      redis_.Command({"DEL", kSaving, kItemsSaving}).IgnoreError();
    }
    // The kills are on their wars' scores, so they leave the stream.
    if (!kills.empty()) {
      redis_.Command(counted).IgnoreError();
    }
    saved += rows;
  }
  return saved;
}

absl::StatusOr<int> Players::Unsaved() {
  // Those in a batch that's being saved, or was and wasn't, aren't saved yet
  // either, and one who's changed since is in both sets, and counts once.
  const absl::StatusOr<std::string> counted =
      redis_.Command({"EVAL", kCountUnion, "2", kUnsaved, kSaving});
  if (!counted.ok()) {
    return counted.status();
  }
  int unsaved = 0;
  if (!absl::SimpleAtoi(*counted, &unsaved)) {
    return absl::InternalError("a count of unsaved players that isn't one");
  }
  return unsaved;
}

absl::StatusOr<int> Players::Duplicates() {
  // By player_items' serial, from V13, which MySQL takes from each row's
  // state, and indexes.
  const absl::StatusOr<Result> counted = database_.Run(
      "SELECT COUNT(*) FROM (SELECT serial FROM player_items "
      "WHERE serial IS NOT NULL GROUP BY serial HAVING COUNT(*) > 1) AS twice");
  if (!counted.ok()) {
    return counted.status();
  }
  int duplicates = 0;
  if (counted->rows.empty() ||
      !absl::SimpleAtoi(counted->rows.front().front(), &duplicates)) {
    return absl::InternalError("a count of duplicates that isn't a number");
  }
  return duplicates;
}

}  // namespace nanommo
