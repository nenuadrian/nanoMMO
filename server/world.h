#ifndef NANOMMO_SERVER_WORLD_H_
#define NANOMMO_SERVER_WORLD_H_

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "content/content.h"
#include "protocol/nanommo.pb.h"
#include "server/duels.h"
#include "server/history.h"
#include "server/inventory.h"
#include "server/trades.h"
#include "server/walking.h"
#include "server/wars.h"
#include "terrain/lands.h"
#include "terrain/paths.h"
#include "terrain/terrain.h"

namespace nanommo {

enum class Motion { kIdle, kWalk, kRun, kAttack, kDead, kSkill };

/// What a player's worn items add up to, worked out as what they wear
/// changes, so that a fight doesn't add it up for each blow.
struct Stats {
  int damage = 0;
  int health = 0;
  int armour = 0;
  /// The weapon in the player's main hand, from the content, or nullptr.
  const items::Weapon* weapon = nullptr;
};

struct Player {
  std::string name;
  /// How they chose to look, which everyone's shown, or none for a character
  /// from before there were looks to choose, whose id picks theirs.
  std::optional<protocol::Looks> looks;
  /// The guild they're in, by its id, or 0 for none, which the server tells
  /// the world, as it's in MySQL.
  uint32_t guild = 0;
  float x = 0.0f;
  float z = 0.0f;
  /// In radians, around the vertical. Zero faces along z, towards the camera.
  float yaw = 0.0f;
  Motion motion = Motion::kIdle;
  uint32_t experience = 0;
  int health = 0;
  /// Which of the world's lands, by its place in kLands.
  int land = 0;
  /// What the player carries and wears.
  Inventory inventory;
  Stats stats;
  /// The skills the player knows, by their ids in the skills file.
  std::set<uint32_t> skills;
  /// The quests the player's on, by their ids in the quests file, with how
  /// many they've killed for each.
  std::map<uint32_t, uint32_t> quests;
  /// The quests they've turned in, which they can't take again.
  std::set<uint32_t> quests_done;
  /// Each change to their quests takes this to the next, as a bag's revision
  /// does, so the server can tell when to send them.
  uint32_t quests_revision = 0;
  /// The monster the player picked, by its id, which their swings and skills
  /// go for, or 0 for none.
  uint32_t target = 0;
  /// The player they picked, by their id, in place of a monster: their
  /// opponent in a duel, or an enemy at war, as nobody else can be. Players
  /// and monsters have ids of their own, so it goes beside the monster.
  uint32_t target_player = 0;
  /// How far in the past their client draws everyone else, which their blows
  /// on another player are checked at.
  std::chrono::nanoseconds behind{0};
  /// The skill the player's using, by its id in the skills file, and how far
  /// into it they are.
  uint32_t skill = 0;
  std::chrono::nanoseconds cast{0};
  /// What the skill does, as it was when it started, with the weapon in hand
  /// then, so that changing the weapon doesn't change how long it takes. And
  /// that weapon, as a weapon skill's blows land only while it's in hand.
  skills::Effect effect;
  const items::Weapon* cast_weapon = nullptr;
  /// Where they stood as the skill started, which its strides go on from.
  float cast_x = 0.0f;
  float cast_z = 0.0f;
  /// How many skills they've started, so that one straight after itself
  /// starts again on the clients.
  uint32_t casts = 0;
  /// The swing that Space goes on into next, by its id, for as long as there's
  /// left to use it, or 0 for the first.
  uint32_t chain = 0;
  std::chrono::nanoseconds chain_left{0};
  /// How long until each skill they've used is ready again, by its id.
  std::map<uint32_t, std::chrono::nanoseconds> cooldowns;
  /// How long until they can use each item again, by its id in the items
  /// file.
  std::map<uint32_t, std::chrono::nanoseconds> item_cooldowns;
  /// While they're dead, how long until they can come back.
  std::chrono::nanoseconds respawn{0};
  /// Where the player walks by themselves, as they asked to: each place in
  /// turn, straight, until they get there, or move, fight or die.
  std::deque<Place> way;
  /// Whether the world moved them itself this tick, along their way, up to a
  /// monster or with a skill, or they're dead, so their keys didn't.
  bool steered = false;
  float input_x = 0.0f;
  float input_z = 0.0f;
  bool input_run = false;
  bool input_attack = false;
  /// A skill the player asked for, which waits until they're free.
  uint32_t input_skill = 0;
  /// What the player's making, by its recipe's id in the recipes file, or 0
  /// for nothing, and how long they've been at it.
  uint32_t crafting = 0;
  std::chrono::nanoseconds crafted{0};
};

/// How long a blow's push takes, fast at first and slowing, as a shove does.
inline constexpr std::chrono::milliseconds kPushTime{200};

/// What pulls a monster that a blow's thrown up back down, in metres a second
/// each second: more than the Earth's, so that it doesn't float.
inline constexpr float kGravity = 14.0f;

/// How fast a blow without a lift of its own sends up a monster that's already
/// in the air, in metres a second: enough to keep it there a moment longer.
inline constexpr float kPop = 2.5f;

/// However many players take turns at it, a monster's thrown up for this long
/// at most, before it falls whatever hits it, and then it stands its ground
/// for a while, so that it gets to fight back.
inline constexpr std::chrono::milliseconds kMostInAir{2000};
inline constexpr std::chrono::milliseconds kSteady{2000};

struct Monster {
  /// By its id in the monsters file.
  uint32_t kind = 0;
  float x = 0.0f;
  float z = 0.0f;
  float yaw = 0.0f;
  Motion motion = Motion::kIdle;
  int level = 1;
  int health = 0;
  int max_health = 0;
  uint32_t experience = 0;
  int damage = 0;
  std::chrono::nanoseconds swing_time{0};
  std::chrono::nanoseconds hit_time{0};
  /// In metres, from its kind's fight.
  float reach = 0.0f;
  float sight = 0.0f;
  float leash = 0.0f;
  float wander = 0.0f;
  /// Its kind's skills, which it uses in place of a swing, each as soon as
  /// it's ready and the player it's fighting is within its range, the first
  /// first.
  std::vector<monsters::Skill> skills;
  /// A boss's phases, and the one it's in, by its place among them. It uses
  /// that phase's skills alone.
  std::vector<monsters::Phase> phases;
  std::size_t phase = 0;
  /// For an add, the boss that called it up, by its id. It doesn't come back
  /// to life, and it goes once its boss is back in its first phase, at home
  /// or alive again.
  uint32_t called_by = 0;
  float home_x = 0.0f;
  float home_z = 0.0f;
  float target_x = 0.0f;
  float target_z = 0.0f;
  std::chrono::nanoseconds rest{0};
  uint32_t prey = 0;
  /// Its threat table: how much each player who's hurt it has, by their id.
  /// It goes for whoever's hurt it most, seen or not, while they're within its
  /// leash.
  std::map<uint32_t, int> threat;
  /// Whether a boss is on its way home, once nobody it was fighting is left,
  /// to start again.
  bool evading = false;
  std::chrono::nanoseconds swing{0};
  /// While its motion is a skill's: the skill, how far into it it is, and
  /// where it stood as it started. The shape stays where it was aimed, the
  /// way the monster faced, so that players can step out of it.
  monsters::Skill casting;
  std::chrono::nanoseconds cast{0};
  float cast_x = 0.0f;
  float cast_z = 0.0f;
  /// How long until each skill it's used is ready again, by its id.
  std::map<uint32_t, std::chrono::nanoseconds> cooldowns;
  /// How long it lies dead before it comes back to life, from its kind, and
  /// while it's dead, how long until it does.
  std::chrono::nanoseconds respawn_time{0};
  std::chrono::nanoseconds respawn{0};
  /// A blow's push, which plays out over a few ticks: where it started, how
  /// far it goes, in metres, and how long it's been going. While it does, the
  /// monster thinks of nothing else, though a swing it started goes on.
  float pushed_from_x = 0.0f;
  float pushed_from_z = 0.0f;
  float push_x = 0.0f;
  float push_z = 0.0f;
  std::chrono::nanoseconds pushed = kPushTime;
  /// While a blow has it in the air: how high it is, in metres, and how fast
  /// it's going up, falling, as it slows, for less than nothing. It does
  /// nothing else until it lands.
  float height = 0.0f;
  float rise = 0.0f;
  /// How long it's been in the air since it was first thrown up, and, once
  /// it's down, how long until it can be thrown again.
  std::chrono::nanoseconds in_air{0};
  std::chrono::nanoseconds steady{0};

  /// A monster with phases is a boss.
  bool boss() const { return !phases.empty(); }
};

/// Someone who stands in one place, such as a guard. NPCs don't move or fight,
/// so they have none of a monster's health or of what it thinks with.
struct Npc {
  std::string name;
  /// By its id in the NPCs file.
  uint32_t kind = 0;
  float x = 0.0f;
  float z = 0.0f;
  float yaw = 0.0f;
  /// Whether players can talk to it.
  bool talks = false;
  /// The skills it teaches, by their ids in the skills file.
  std::set<uint32_t> teaches;
};

/// A line the world has for one of its players, such as what a monster
/// they killed dropped, for the server to send them.
struct Notice {
  uint32_t player = 0;
  std::string text;
  /// CHANNEL_LAND for what everyone in the land hears, which the clients
  /// show apart.
  protocol::Channel channel = protocol::CHANNEL_WORLD;
};

/// Something a player did, or that happened to them, for the game's
/// analytics and for the players near them to see: what, who and where, and
/// what it was about, by its ids in the content.
struct Event {
  enum class Kind {
    kKill,
    kDeath,
    kLoot,
    kPrize,
    kLevel,
    kTake,
    kTurnIn,
    kUse,
    kHit,
    kHurt,
    kHeal,
    kCraft
  };
  Kind kind = Kind::kKill;
  std::string player;
  /// The player's id in the world, which the clients know them by.
  uint32_t player_id = 0;
  /// By its place in kLands.
  int land = 0;
  /// The copy of the land it happened in, which Copies fills in.
  uint32_t copy = 0;
  float x = 0.0f;
  float z = 0.0f;
  /// The monster killed, or that killed or hit the player.
  uint32_t monster = 0;
  /// The monster the player hit, by its id in the world.
  uint32_t monster_id = 0;
  /// An item dropped, given for a quest, used or made, and how many.
  uint32_t item = 0;
  uint32_t count = 0;
  uint32_t quest = 0;
  /// The level a player's reached.
  uint32_t level = 0;
  /// The health a hit took, or that a potion or a skill gave back.
  uint32_t amount = 0;
};

/// The most health the player can have: their level's, and what they wear
/// adds.
int MaxHealthOf(const Player& player);

/// What the player holds in their main hand, by its id in the items file, or
/// zero.
uint32_t MainHandOf(const Player& player);

/// What else the player wears, in each slot from the off hand to the feet that
/// has something in it, by its ids in the items file.
std::vector<uint32_t> WornOf(const Player& player);

class World {
 public:
  /// As walking.h has them, which the monsters go at too.
  static constexpr float kWalkSpeed = nanommo::kWalkSpeed;
  static constexpr float kRunSpeed = nanommo::kRunSpeed;

  /// A player who swings or uses a skill at the monster they picked, while
  /// it's out of reach, runs up to this share of the reach first, so that a
  /// monster that steps back is still within it.
  static constexpr float kCloseIn = 0.8f;

  /// How long the dead wait before they can come back. A monster comes back
  /// once it's over, and a player when they ask to.
  static constexpr std::chrono::seconds kRespawnTime{10};

  /// How far from a boss its adds come, in metres.
  static constexpr float kAddsAround = 3.0f;

  /// How much of a boss's health a player has to have taken, in percent, for
  /// its experience and a roll of its loot when it dies.
  static constexpr int kEnoughHelp = 5;

  /// How long before a boss comes back to life its land hears that it will.
  static constexpr std::chrono::seconds kRisesSoon{30};

  static constexpr float kTalkReach = nanommo::kTalkReach;

  /// Two players trade within talking reach, as close as they'd stand to talk.
  static constexpr float kTradeReach = kTalkReach;

  /// Players fight with the content's weapons and skills, so in a world
  /// without content, nobody can. Content doesn't change as the game runs, so
  /// the world's copies of its lands share one.
  /// The world's copies of a land share its paths, as they're the same land;
  /// without them, the world makes its own.
  explicit World(
      uint32_t seed = std::mt19937::default_seed, Terrain terrain = Terrain(),
      std::shared_ptr<const Content> content = std::make_shared<Content>(),
      std::shared_ptr<Paths> paths = nullptr);

  void Join(uint32_t id, Player player);

  void Leave(uint32_t id);

  void SetInput(uint32_t id, float x, float z, bool run, bool attack = false);

  /// The monster, by its id, that the player's swings and skills go for from
  /// now on, or 0 for none. Or a player they may hurt, by their id, in its
  /// place: anyone else is no target.
  void Pick(uint32_t id, uint32_t monster, uint32_t player = 0);

  /// The player's client draws everyone else this long ago, so their blows
  /// on another player are checked against where that player was then.
  /// Inputs believes Inputs::kMostRewind of what a client says, at most.
  void SetBehind(uint32_t id, std::chrono::nanoseconds behind);

  /// A dead player comes back in the middle of their land, at full health,
  /// once their wait is over.
  void Respawn(uint32_t id);

  /// The player walks there by themselves, around what's in the way, or as
  /// near as the way goes, if they can stand there.
  void WalkTo(uint32_t id, Place to);

  /// The player uses the skill as soon as they're free, if they know it and
  /// it's ready.
  void UseSkill(uint32_t id, uint32_t skill);

  /// The NPC, a trainer in this land, teaches the player the skill, or the
  /// error says why not, in words for the player.
  absl::Status Learn(uint32_t id, uint32_t npc, uint32_t skill);

  /// The player takes the quest from the NPC, who gives it, in this land, or
  /// the error says why not, in words for the player.
  absl::Status TakeQuest(uint32_t id, uint32_t npc, uint32_t quest);

  /// The player turns the quest in to the NPC, who gave it, once they've
  /// killed as many as it asks for, for its experience, and its prizes in
  /// their bag, or the error says why not, in words for the player.
  absl::Status TurnIn(uint32_t id, uint32_t npc, uint32_t quest);

  /// Moves the player's items around, as Inventory::Move does, wearing them
  /// and taking them off, or the error says why not, in words for the player.
  /// Nothing goes to or from a place they offer in a trade.
  absl::Status MoveItem(uint32_t id, const protocol::Place& from,
                        const protocol::Place& to, uint32_t count = 0);

  /// Throws away all of what's at the place, unless they offer it in a trade.
  absl::Status DestroyItem(uint32_t id, const protocol::Place& place);

  /// The player starts making the recipe, wherever they are, which takes its
  /// seconds, if they're of its level and their bag has all it takes, outside
  /// what they offer in a trade, and room for what it makes. Or the error says
  /// why not, in words for the player. Moving, a swing or a skill, a blow, a
  /// trade or a duel stops it, and they lose nothing.
  absl::Status Craft(uint32_t id, uint32_t recipe);

  /// Stops what the player's making, if anything, and they lose nothing.
  void StopCrafting(uint32_t id);

  /// The player uses one of what's in the slot of their bag, such as drinks a
  /// potion, unless they offer it in a trade, or the error says why not, in
  /// words for the player.
  absl::Status UseItem(uint32_t id, uint32_t slot);

  /// The player asks the one of that name here, whatever its case, within
  /// kTradeReach, to trade, or says yes to them asking first, which opens the
  /// trade. Or the error says why not, in words for the player.
  absl::Status AskToTrade(uint32_t id, std::string_view name);

  /// What the player offers in their trade, as Trades::Offer has it.
  absl::Status OfferToTrade(uint32_t id, const protocol::TradeOffer& offer);

  /// Whether the player's ready to trade, at the revision of the trade they
  /// saw. Once both are, within kTradeReach, they trade, there and then, and
  /// it returns who with, or 0 until then. Or the error says why not.
  absl::StatusOr<uint32_t> ReadyToTrade(uint32_t id, bool ready,
                                        uint32_t revision);

  /// Calls the player's trade off, for both of them.
  void CancelTrade(uint32_t id);

  /// The player's trade, as their client gets it: who with, by name, what
  /// each side offers, and its revision, or nobody, once it's over.
  protocol::Trade TradeOf(uint32_t id) const;

  /// The players whose trade has opened, changed or ended since the last
  /// time, which it hands over.
  std::set<uint32_t> TakeTradeNews();

  /// The player asks the one of that name here, whatever its case, within
  /// Duels::kReach, to duel, or says yes to them asking first. Or the error
  /// says why not, in words for the player.
  absl::Status AskToDuel(uint32_t id, std::string_view name);

  /// The player says yes or no to the one who asked them to duel. A yes puts
  /// the flag down where the one who asked stands, and starts the countdown.
  absl::Status AnswerDuel(uint32_t id, bool yes);

  /// The player gives their duel up, and the other side wins it, or before
  /// it's started, calls it off.
  absl::Status Yield(uint32_t id);

  /// Ends the player's duel, however far it's got, with nobody winning, as
  /// when the world hands its players over. Returns who it was with, or 0.
  uint32_t CallOffDuel(uint32_t id);

  /// The player's duel, as their client gets it: who with, how far it's got,
  /// and its flag, or nobody, once it's over.
  protocol::Duel DuelOf(uint32_t id) const;

  /// The players whose duel has been asked for, counted down, started or
  /// ended since the last time, which it hands over.
  std::set<uint32_t> TakeDuelNews();

  /// The player's guild, by its id, or 0 for none.
  void SetGuild(uint32_t id, uint32_t guild);

  /// The wars between the world's guilds, in place of those before. Members
  /// of two guilds at war can hurt each other anywhere.
  void SetWars(const std::vector<War>& wars);

  /// Whether the player may hurt the other: their opponent in a duel that's
  /// started, or a member of the guild at war with theirs.
  bool Hostile(uint32_t id, uint32_t other) const;

  /// The players killed by an enemy at war since the last time, which it
  /// hands over.
  std::vector<WarKill> TakeWarKills();

  uint32_t Spawn(Monster monster);

  /// NPCs have ids of their own, apart from the monsters'.
  uint32_t Spawn(Npc npc);

  void Step(std::chrono::nanoseconds time);

  /// The lines for players since the last time, which it hands over.
  std::vector<Notice> TakeNotices();

  /// What's happened since the last time, which it hands over.
  std::vector<Event> TakeEvents();

  const std::map<uint32_t, Player>& players() const { return players_; }
  const std::map<uint32_t, Monster>& monsters() const { return monsters_; }
  const std::map<uint32_t, Npc>& npcs() const { return npcs_; }
  const Terrain& terrain() const { return terrain_; }

 private:
  /// After what the player wears has changed: works out their stats again.
  void Dress(Player& player) const;
  void Act(uint32_t id, Player& player, std::chrono::nanoseconds time);
  void Move(Player& player, std::chrono::nanoseconds time);
  /// Moves the player along the direction, at most a step of the speed long,
  /// less for a shorter one.
  void Go(Player& player, float x, float z, bool run,
          std::chrono::nanoseconds time);
  /// Runs the player towards what they picked, to within the reach.
  void Approach(Player& player, float reach, std::chrono::nanoseconds time);
  /// Runs the player along their way, a step at a time.
  void Follow(Player& player, std::chrono::nanoseconds time);
  /// Where what the player picked is, while it's alive and within reach: the
  /// monster, or the player they may hurt, where they saw them. Or nothing.
  std::optional<Place> Target(const Player& player, float reach) const;
  /// Where the player's client drew the other player, as far behind as it
  /// draws everyone, from the history: or where they are, for one that's not
  /// behind.
  Place Seen(const Player& player, uint32_t other) const;
  void Aim(Player& player, float reach);
  /// What the skill does when this player uses it: a swing's reach, damage
  /// and timing are the weapon's in their hand.
  skills::Effect EffectOf(const Player& player,
                          const skills::Skill& skill) const;
  /// The skill, if the player can use it now: they know it and it's ready. A
  /// swing needs a weapon, and a skill that hurts one monster needs a living
  /// one picked, so that its cooldown isn't spent on the air, while a swing
  /// goes at the air.
  const skills::Skill* Usable(const Player& player, uint32_t id) const;
  void Start(Player& player, const skills::Skill& skill);
  /// The skill's blow, which hurts and pushes what it reaches. A skill that
  /// lands more than once shares its damage out between its blows, and heals
  /// with the first.
  void Land(uint32_t id, Player& player, const skills::Effect& effect,
            const skills::Blow& blow, int index, int blows);
  /// Takes the player as far along the skill's strides as it's got to, from
  /// where they started it.
  void Stride(Player& player, const skills::Effect& effect);
  /// Starts a push away from the player, by metres, which Slide plays out.
  void Push(const Player& player, Monster& monster, float metres);
  /// Moves the monster on along its push, and says whether one's going on.
  bool Slide(Monster& monster, std::chrono::nanoseconds time);
  /// Throws the monster up, to the height in metres, or, already in the air,
  /// down for less than nothing, and a little further up without one.
  static void Throw(Monster& monster, float lift);
  /// Moves the monster up or down as it flies, and says whether it's still in
  /// the air.
  static bool Fly(Monster& monster, std::chrono::nanoseconds time);
  void Hurt(uint32_t id, Player& player, uint32_t monster_id, Monster& monster,
            int damage);
  /// A blow on the player from another who may hurt them, by their id. From
  /// an enemy at war, it can kill them, as a monster's can. From their
  /// opponent in a duel, one that would leave them no health leaves them one,
  /// and they've lost.
  void Hurt(uint32_t from, uint32_t id, Player& player, int damage);
  /// The war between the two players' guilds, or nullptr.
  const War* WarBetween(const Player& player, const Player& other) const;
  /// Gives the player back as much of the health as they're missing.
  void Heal(uint32_t id, Player& player, int health);
  /// The player's part in the monster's death: its experience, what it drops
  /// for them, and a kill for their quests.
  void Reward(uint32_t id, Player& player, const Monster& monster);
  /// What the monster the player's just killed drops, into their bag, and its
  /// gold.
  void Loot(uint32_t id, Player& player, const Monster& monster);
  /// Puts count of the item into the bag, each with rolls of its own, and
  /// returns how many fitted.
  uint32_t Give(Inventory& bag, uint32_t item, uint32_t count);
  /// The player's bag with the recipe made: all it takes taken from the bag,
  /// outside what they offer in a trade, and what it makes given. Or why not,
  /// in words for the player.
  absl::StatusOr<Inventory> Made(uint32_t id, const Player& player,
                                 const recipes::Recipe& recipe);
  /// What the player's making goes on, and once it's taken its recipe's
  /// seconds, it's made.
  void Make(uint32_t id, Player& player, std::chrono::nanoseconds time);
  /// Stops what the player's making, if anything, and says so.
  void Interrupt(uint32_t id, Player& player);
  /// The monster the player's just killed, for each of their quests that
  /// asks for its kind, until they've killed as many as it asks.
  void Count(uint32_t id, Player& player, const Monster& monster);
  void Act(Monster& monster, std::chrono::nanoseconds time);
  /// The adds of the boss's phase, by their places among its phases, round
  /// it, unless it's died since.
  void CallAdds(uint32_t id, std::size_t phase);
  /// A line everyone in the land hears about the boss, after its name, such as
  /// "The Bone Colossus has risen."
  void Announce(const Monster& boss, std::string_view news);
  void Hit(Monster& monster);
  /// A monster's blow on the player, from a swing or a skill: what they wear
  /// takes some of it, down to one, and with no health left, they die.
  void Hit(uint32_t id, Player& player, const Monster& monster, int blow);
  /// The monster's skill, once its shape has shown for long enough: on each
  /// living player in the shape where it was aimed, where they stand now.
  void LandSkill(const Monster& monster);
  /// An event of the kind, for the player, by their id, where they are.
  Event& Record(Event::Kind kind, uint32_t id, const Player& player);
  /// After everyone's acted: the duels' asks and countdowns run down, and a
  /// duellist who's dead, or too far from the flag, loses.
  void Referee(std::chrono::nanoseconds time);
  /// The player's lost their duel, which is over, and both hear who won.
  void Lose(uint32_t id);

  Terrain terrain_;
  std::shared_ptr<const Content> content_;
  std::shared_ptr<Paths> paths_;
  std::map<uint32_t, Player> players_;
  std::map<uint32_t, Monster> monsters_;
  uint32_t next_monster_ = 1;
  std::map<uint32_t, Npc> npcs_;
  uint32_t next_npc_ = 1;
  std::mt19937 random_;
  std::vector<Notice> notices_;
  std::vector<Event> events_;
  /// The bosses that have gone into a phase this step, by their ids, and the
  /// phase, whose adds come after the players' blows.
  std::vector<std::pair<uint32_t, std::size_t>> calls_;
  /// Between the players here, as both have to be.
  Trades trades_;
  Duels duels_;
  /// The wars between the world's guilds, each by both its sides, as a guild
  /// is in one at a time.
  std::map<uint32_t, War> wars_;
  std::vector<WarKill> war_kills_;
  /// How long the world's run, a step at a time, which its history goes by.
  std::chrono::nanoseconds clock_{0};
  /// Where each player here has been, after each step, for the last second.
  History history_;
};

}  // namespace nanommo

#endif  // NANOMMO_SERVER_WORLD_H_
