#ifndef NANOMMO_CLIENT_FX_H_
#define NANOMMO_CLIENT_FX_H_

#include <raylib.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "client/animator.h"
#include "client/atmosphere.h"
#include "client/characters.h"
#include "content/items.h"
#include "content/skills.h"
#include "terrain/terrain.h"

namespace nanommo {

/// What the skills show besides the players' moves: their fx. Each is worked
/// out from how long ago the skill started, so it looks the same at any frame
/// rate.
class Fx {
 public:
  /// While a blow that's landed holds things still, they go at this share of
  /// their speed, and then catch up, faster, over this many times as long.
  static constexpr float kStill = 0.05f;
  static constexpr float kCatchUp = 1.5f;
  /// How far apart the copies a fast move leaves are, in seconds.
  static constexpr float kGhostGap = 0.05f;

  /// The shards and the spikes are solid, lit by the world's shader. A swing
  /// lands when the weapon in the hand says, from the items.
  Fx(const SkillTable& skills, Shader lit, const ItemTable& items = {});
  ~Fx();

  Fx(const Fx&) = delete;
  Fx& operator=(const Fx&) = delete;

  /// Follows the skills the players are using, and keeps each going until
  /// its fx are over, as a ring spreads on after the move's done.
  void Update(const std::vector<Figure>& figures, float seconds);

  /// Holds the players back where a blow of theirs has just landed, and what
  /// it landed on with them, for the weight of the hit.
  void Hold(std::vector<Figure>& figures) const;

  /// The shards and the spikes.
  void DrawSolid(const Terrain& terrain) const;

  /// The copies fast moves leave behind them, which go over what's solid.
  void DrawGhosts(const Characters& characters) const;

  /// The light, which goes over what's behind it, and blooms.
  void DrawLight(const Camera3D& camera, const Terrain& terrain,
                 const Atmosphere& atmosphere,
                 const Characters& characters) const;

  /// How far the camera's shaken, at this moment, by the skills near it.
  Vector3 Shake(Vector3 camera) const;

  /// How far in our own camera goes, in metres, for a skill we're using.
  float Zoom() const;

  /// How many skills' fx are going.
  std::size_t casts() const { return casts_.size(); }

 private:
  struct Cast {
    /// The player as we last saw them using it.
    Figure figure;
    /// Seconds since it started, and how far its move and fx have got, which
    /// the blows that land hold back.
    float clock = 0.0f;
    float time = 0.0f;
    /// Where the monster they picked is, if they picked one.
    std::optional<Vector3> target;
    /// Each cast has sparks of its own.
    uint32_t seed = 0;
    /// Whether each blow it's got to so far landed, in order.
    std::vector<bool> landed;
    /// What its blows landed on, by id, held still with the player.
    std::set<uint32_t> struck;
    /// Where the player's been, by the time of the cast's move.
    std::deque<std::pair<float, Vector3>> path;
  };

  /// When each of the skill's blows lands, in seconds, for the player, and
  /// how much longer than the skill's stop it holds things still.
  struct Beat {
    float at = 0.0f;
    float weight = 1.0f;
  };
  std::vector<Beat> BlowsOf(const skills::Skill& skill,
                            const Figure& figure) const;
  /// How far the cast's held back now, in seconds.
  float Lag(const Cast& cast) const;
  /// Whether the blow lands, as we see it: what it goes for is alive and
  /// within its reach. What it lands on goes into the cast's struck.
  bool Lands(Cast& cast, const skills::Skill& skill,
             const std::vector<Figure>& figures) const;

  std::map<uint32_t, skills::Skill> skills_;
  /// Each weapon's swing, by its item's id.
  std::map<uint32_t, items::Weapon> weapons_;
  /// When each skill's fx are over, in seconds, by its id.
  std::map<uint32_t, float> ends_;
  /// By the player's id.
  std::map<uint32_t, Cast> casts_;
  uint32_t seeds_ = 0;
  Shader light_;
  Model shard_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_FX_H_
