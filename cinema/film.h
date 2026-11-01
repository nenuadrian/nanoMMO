#ifndef NANOMMO_CINEMA_FILM_H_
#define NANOMMO_CINEMA_FILM_H_

#include <raylib.h>

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "cinema/film.pb.h"
#include "content/content.h"
#include "protocol/nanommo.pb.h"
#include "terrain/terrain.h"

namespace nanommo::cinema {

/// The shots in a text file, as the content files are written.
absl::StatusOr<Film> ReadFilm(const std::string& path);

/// The land a shot is in, by its place in kLands.
int LandOf(const Shot& shot);

/// The land's terrain, as the server builds it for the first world.
Terrain TerrainOf(int land);

/// How fast the game runs at that second of the shot.
float SpeedAt(const Shot& shot, float seconds);

/// The game's clock, which runs as fast as the shot's pace says against the
/// film's.
class Clock {
 public:
  explicit Clock(const Shot& shot) : shot_(shot), game_(shot.from()) {}

  /// Moves the film on by the seconds, and gives how far the game moved.
  float Advance(float seconds);

  float film() const { return film_; }
  float game() const { return game_; }

 private:
  const Shot& shot_;
  float film_ = 0.0f;
  float game_ = 0.0f;
};

/// The point in the land, with its height worked out from the ground or the
/// sea.
Vector3 Place(const Point& point, const Terrain& terrain);

/// Where the camera is at that second of the shot, and what it looks at.
Camera3D CameraAt(const Shot& shot, float seconds, const Terrain& terrain);

/// A shot's world, played out on the game's clock. Players go where their
/// legs take them and use their skills when the shot says. Monsters stand,
/// walk their legs, or charge the player they're set on and swing at them in
/// reach, and the skills' hits land on them as the server's would: knocked
/// back by a push, falling at their last hit.
class Stage {
 public:
  Stage(const Shot& shot, const Content& content);

  /// Plays the world on to that moment, which can't be before the last, and
  /// gives it as the server would send it.
  protocol::World At(float game);

 private:
  struct Foe {
    Monster spec;
    Vector2 at;
    float yaw = 0.0f;
    uint32_t hits = 1;
    bool fallen = false;
    protocol::Motion motion = protocol::MOTION_IDLE;
    /// While it's knocked back: from where, to where, and since when.
    Vector2 pushed_from{};
    Vector2 pushed_to{};
    float pushed_at = -1.0f;
  };

  /// What a cast does, from its skill, or its weapon's swing for a skill
  /// that swings it.
  struct Blow {
    float lasts = 0.0f;
    float lands = 0.0f;
    float reach = 0.0f;
    bool all = false;
    float push = 0.0f;
    float arc = 0.0f;
  };
  Blow BlowOf(const Player& player, const Cast& cast) const;

  /// Where the player is at that moment of the game.
  Vector2 PlayerAt(int player, float game) const;

  void Step(float from, float to);
  void Land(int player, const Cast& cast, float game);

  const Shot& shot_;
  const Content& content_;
  std::vector<Foe> foes_;
  float now_ = 0.0f;
};

}  // namespace nanommo::cinema

#endif  // NANOMMO_CINEMA_FILM_H_
