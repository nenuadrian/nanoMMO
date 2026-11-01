#ifndef NANOMMO_CLIENT_FX_H_
#define NANOMMO_CLIENT_FX_H_

#include <raylib.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "client/animator.h"
#include "client/atmosphere.h"
#include "client/characters.h"
#include "content/skills.h"
#include "terrain/terrain.h"

namespace nanommo {

/// What the skills show besides the players' moves: their fx. Each is worked
/// out from how long ago the skill started, so it looks the same at any frame
/// rate.
class Fx {
 public:
  explicit Fx(const SkillTable& skills);
  ~Fx();

  Fx(const Fx&) = delete;
  Fx& operator=(const Fx&) = delete;

  /// Follows the skills the players are using, and keeps each going until
  /// its fx are over, as a ring spreads on after the move's done.
  void Update(const std::vector<Figure>& figures, float seconds);

  /// The light, which goes over what's behind it, and blooms.
  void DrawLight(const Camera3D& camera, const Terrain& terrain,
                 const Atmosphere& atmosphere,
                 const Characters& characters) const;

  /// How far the camera's shaken, at this moment, by the skills near it.
  Vector3 Shake(Vector3 camera) const;

  /// How many skills' fx are going.
  std::size_t casts() const { return casts_.size(); }

 private:
  struct Cast {
    /// The player as we last saw them using it.
    Figure figure;
    /// Seconds since it started.
    float time = 0.0f;
    /// Where the monster they picked is, if they picked one.
    std::optional<Vector3> target;
    /// Each cast has sparks of its own.
    uint32_t seed = 0;
  };

  std::map<uint32_t, skills::Skill> skills_;
  /// When each skill's fx are over, in seconds, by its id.
  std::map<uint32_t, float> ends_;
  /// By the player's id.
  std::map<uint32_t, Cast> casts_;
  uint32_t seeds_ = 0;
  Shader light_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_FX_H_
