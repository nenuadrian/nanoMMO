#include "cinema/film.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "content/text.h"
#include "terrain/lands.h"

namespace nanommo::cinema {
namespace {

// The server's first world, whose seed each land adds its own to.
constexpr uint32_t kWorldSeed = 1;
// The lens when no key says.
constexpr float kFov = 45.0f;
// In metres a second, as the server's World moves players and monsters.
constexpr float kWalkSpeed = 1.8f;
constexpr float kRunSpeed = 3.1f;

float Smooth(float s) { return s * s * (3.0f - 2.0f * s); }

// From linear, at 0, to smoothed in and out, at 1.
float Eased(float s, float ease) {
  s = std::clamp(s, 0.0f, 1.0f);
  return s + ease * (Smooth(s) - s);
}

// Through b at 0 and c at 1, curving as a and d say, so that a camera passing
// through several keys turns smoothly at each.
Vector3 CatmullRom(Vector3 a, Vector3 b, Vector3 c, Vector3 d, float u) {
  const float u2 = u * u;
  const float u3 = u2 * u;
  const auto term = [&](float pa, float pb, float pc, float pd) {
    return 0.5f * (2.0f * pb + (pc - pa) * u +
                   (2.0f * pa - 5.0f * pb + 4.0f * pc - pd) * u2 +
                   (3.0f * pb - pa - 3.0f * pc + pd) * u3);
  };
  return {term(a.x, b.x, c.x, d.x), term(a.y, b.y, c.y, d.y),
          term(a.z, b.z, c.z, d.z)};
}

float Radians(float degrees) { return degrees * DEG2RAD; }

Camera3D Lens(Vector3 eye, Vector3 look, float fov) {
  return {.position = eye,
          .target = look,
          .up = {0.0f, 1.0f, 0.0f},
          .fovy = fov,
          .projection = CAMERA_PERSPECTIVE};
}

Camera3D Along(const google::protobuf::RepeatedPtrField<Key>& keys,
               float seconds, float ease, const Terrain& terrain) {
  // Each key's eye, look and lens, with what a key leaves out carried on.
  std::vector<Vector3> eyes, looks;
  std::vector<float> fovs, times;
  for (const Key& key : keys) {
    eyes.push_back(Place(key.eye(), terrain));
    looks.push_back(key.has_look()  ? Place(key.look(), terrain)
                    : looks.empty() ? Vector3Add(eyes.back(), {0, 0, 1})
                                    : looks.back());
    fovs.push_back(key.fov() > 0.0f ? key.fov()
                   : fovs.empty()   ? kFov
                                    : fovs.back());
    times.push_back(key.at());
  }
  const int n = static_cast<int>(keys.size());
  if (n == 1) {
    return Lens(eyes[0], looks[0], fovs[0]);
  }
  // Eased over the whole path, so that it sets off and stops gently, rather
  // than at each key.
  const float first = times.front();
  const float span = std::max(times.back() - first, 1e-3f);
  const float t = first + span * Eased((seconds - first) / span, ease);
  int i = 0;
  while (i < n - 2 && t > times[i + 1]) {
    ++i;
  }
  const float u = std::clamp(
      (t - times[i]) / std::max(times[i + 1] - times[i], 1e-3f), 0.0f, 1.0f);
  const auto curve = [&](const std::vector<Vector3>& p) {
    return CatmullRom(p[std::max(i - 1, 0)], p[i], p[i + 1],
                      p[std::min(i + 2, n - 1)], u);
  };
  return Lens(curve(eyes), curve(looks), Lerp(fovs[i], fovs[i + 1], u));
}

Camera3D Round(const Orbit& orbit, float share, const Terrain& terrain) {
  const Vector3 center = Place(orbit.center(), terrain);
  const float angle = Radians(Lerp(orbit.from(), orbit.to(), share));
  const Vector3 eye = {center.x + orbit.radius() * std::sin(angle),
                       center.y + orbit.up(),
                       center.z + orbit.radius() * std::cos(angle)};
  return Lens(eye, center, orbit.fov() > 0.0f ? orbit.fov() : kFov);
}

// Where someone is, which way they face, and whether they're on their way
// somewhere, at a moment of the game, after the legs they started by then.
struct Spot {
  Vector2 at;
  float yaw = 0.0f;
  protocol::Motion motion = protocol::MOTION_IDLE;
};

Spot Walked(const Point& start, float yaw,
            const google::protobuf::RepeatedPtrField<Leg>& legs, float game) {
  Spot spot{.at = {start.x(), start.z()}, .yaw = yaw};
  // When they're free to set off on the next leg: once they've arrived from
  // the one before.
  float free = -1e9f;
  for (const Leg& leg : legs) {
    const float off = std::max(leg.at(), free);
    if (game < off) {
      break;
    }
    const Vector2 to = {leg.to().x(), leg.to().z()};
    const Vector2 way = Vector2Subtract(to, spot.at);
    const float distance = Vector2Length(way);
    const float speed = leg.run() ? kRunSpeed : kWalkSpeed;
    const float arrive = off + distance / speed;
    if (distance > 1e-3f) {
      spot.yaw = std::atan2(way.x, way.y);
    }
    if (game < arrive) {
      spot.at = Vector2Lerp(spot.at, to, (game - off) / (arrive - off));
      spot.motion = leg.run() ? protocol::MOTION_RUN : protocol::MOTION_WALK;
      return spot;
    }
    spot.at = to;
    free = arrive;
  }
  return spot;
}

}  // namespace

absl::StatusOr<Film> ReadFilm(const std::string& path) {
  absl::StatusOr<std::string> text = ReadFile(path);
  if (!text.ok()) {
    return text.status();
  }
  Film film;
  if (absl::Status parsed = ParseText(*text, film); !parsed.ok()) {
    return absl::InvalidArgumentError(
        absl::StrCat(path, ": ", parsed.message()));
  }
  for (const Shot& shot : film.shots()) {
    if (!IsLand(shot.land())) {
      return absl::InvalidArgumentError(
          absl::StrCat(shot.id(), ": no land called ", shot.land()));
    }
    if (shot.camera().empty() && !shot.has_orbit()) {
      return absl::InvalidArgumentError(
          absl::StrCat(shot.id(), ": no camera keys and no orbit"));
    }
  }
  return film;
}

int LandOf(const Shot& shot) { return LandNamed(shot.land()); }

Terrain TerrainOf(int land) {
  return Terrain(kWorldSeed + kLands[land].seed, kLands[land].wading);
}

float SpeedAt(const Shot& shot, float seconds) {
  const auto& pace = shot.pace();
  if (pace.empty()) {
    return 1.0f;
  }
  if (seconds <= pace[0].at()) {
    return pace[0].speed();
  }
  for (int i = 0; i + 1 < pace.size(); ++i) {
    const Pace& from = pace[i];
    const Pace& to = pace[i + 1];
    if (seconds < to.at()) {
      const float share = (seconds - from.at()) / (to.at() - from.at());
      return Lerp(from.speed(), to.speed(), Smooth(share));
    }
  }
  return pace[pace.size() - 1].speed();
}

float Clock::Advance(float seconds) {
  // In small steps, as the pace can change within a frame.
  constexpr int kSteps = 16;
  const float step = seconds / kSteps;
  float moved = 0.0f;
  for (int i = 0; i < kSteps; ++i) {
    moved += step * SpeedAt(shot_, film_ + (i + 0.5f) * step);
  }
  film_ += seconds;
  game_ += moved;
  return moved;
}

Vector3 Place(const Point& point, const Terrain& terrain) {
  const float base =
      point.sea() ? Terrain::kSeaLevel : terrain.Height(point.x(), point.z());
  return {point.x(), base + point.up(), point.z()};
}

Camera3D CameraAt(const Shot& shot, float seconds, const Terrain& terrain) {
  if (!shot.camera().empty()) {
    return Along(shot.camera(), seconds, shot.ease(), terrain);
  }
  return Round(shot.orbit(), Eased(seconds / shot.seconds(), shot.ease()),
               terrain);
}

Stage::Stage(const Shot& shot, const Content& content)
    : shot_(shot), content_(content) {
  for (const Monster& monster : shot.monsters()) {
    foes_.push_back({.spec = monster,
                     .at = {monster.at().x(), monster.at().z()},
                     .yaw = Radians(monster.yaw()),
                     .hits = std::max(monster.hits(), 1u)});
  }
  // A pack spreads out like the seeds of a sunflower, each a golden angle
  // round from the one before, and a little further out.
  constexpr float kGolden = 2.39996f;
  for (const Pack& pack : shot.packs()) {
    for (uint32_t i = 0; i < pack.count(); ++i) {
      const float out = pack.radius() * std::sqrt((i + 0.5f) / pack.count());
      const float turn = kGolden * static_cast<float>(i);
      Monster monster;
      monster.set_id(pack.first() + i);
      monster.set_kind(pack.kind());
      monster.mutable_at()->set_x(pack.center().x() + out * std::sin(turn));
      monster.mutable_at()->set_z(pack.center().z() + out * std::cos(turn));
      monster.set_face(pack.face());
      monster.set_charge(pack.charge());
      // Not all at once: each a little after the one inside it.
      monster.set_charge_at(pack.charge_at() + 0.04f * static_cast<float>(i));
      monster.set_hits(pack.hits());
      monster.set_level(pack.level());
      foes_.push_back({.spec = monster,
                       .at = {monster.at().x(), monster.at().z()},
                       .hits = std::max(monster.hits(), 1u)});
    }
  }
}

Stage::Blow Stage::BlowOf(const Player& player, const Cast& cast) const {
  const auto skill = content_.skills.by_id.find(cast.skill());
  if (skill == content_.skills.by_id.end()) {
    return {};
  }
  const skills::Effect& effect = skill->second.effect();
  if (effect.weapon()) {
    const auto item = content_.items.by_id.find(player.weapon());
    if (item == content_.items.by_id.end()) {
      return {};
    }
    const items::Weapon& weapon = item->second.weapon();
    return {.lasts = weapon.swing_ms() / 1000.0f,
            .lands = weapon.hit_ms() / 1000.0f,
            .reach = weapon.reach()};
  }
  return {.lasts = effect.cast_ms() / 1000.0f,
          .lands = effect.hit_ms() / 1000.0f,
          .reach = effect.reach(),
          .all = effect.all(),
          .push = effect.push(),
          .arc = effect.arc()};
}

Vector2 Stage::PlayerAt(int player, float game) const {
  const Player& who = shot_.players(player);
  return Walked(who.at(), Radians(who.yaw()), who.legs(), game).at;
}

void Stage::Land(int player, const Cast& cast, float game) {
  const Player& who = shot_.players(player);
  const Blow blow = BlowOf(who, cast);
  if (blow.reach <= 0.0f) {
    return;
  }
  const Vector2 from = PlayerAt(player, game);
  const auto alive = [](const Foe& foe) { return !foe.fallen; };
  std::vector<Foe*> hit;
  if (blow.all) {
    // Facing the target, if there's one, for a skill that hits a slice.
    float facing = Radians(who.yaw());
    for (const Foe& foe : foes_) {
      if (foe.spec.id() == cast.target()) {
        facing = std::atan2(foe.at.x - from.x, foe.at.y - from.y);
      }
    }
    for (Foe& foe : foes_) {
      const Vector2 way = Vector2Subtract(foe.at, from);
      if (!alive(foe) || Vector2Length(way) > blow.reach) {
        continue;
      }
      const float off =
          std::remainder(std::atan2(way.x, way.y) - facing, 2.0f * PI);
      if (blow.arc <= 0.0f || std::abs(off) <= Radians(blow.arc) / 2) {
        hit.push_back(&foe);
      }
    }
  } else {
    // The target, or else the nearest in reach, with a little to spare, as a
    // film's monsters don't stand where the server would let them.
    Foe* target = nullptr;
    Foe* nearest = nullptr;
    float best = blow.reach + 1.0f;
    for (Foe& foe : foes_) {
      if (!alive(foe)) {
        continue;
      }
      const float d = Vector2Distance(foe.at, from);
      if (foe.spec.id() == cast.target() && d <= blow.reach + 2.0f) {
        target = &foe;
      }
      if (d < best) {
        best = d;
        nearest = &foe;
      }
    }
    if (Foe* struck = target != nullptr ? target : nearest) {
      hit.push_back(struck);
    }
  }
  for (Foe* foe : hit) {
    foe->fallen = --foe->hits == 0;
    if (blow.push > 0.0f) {
      const Vector2 away = Vector2Normalize(Vector2Subtract(foe->at, from));
      foe->pushed_from = foe->at;
      foe->pushed_to = Vector2Add(foe->at, Vector2Scale(away, blow.push));
      foe->pushed_at = game;
    }
  }
}

void Stage::Step(float from, float to) {
  // The blows that land in the step.
  for (int p = 0; p < shot_.players_size(); ++p) {
    const Player& who = shot_.players(p);
    for (const Cast& cast : who.casts()) {
      const float lands = cast.at() + BlowOf(who, cast).lands;
      if (lands > from && lands <= to) {
        Land(p, cast, lands);
      }
    }
  }
  constexpr float kPushTime = 0.3f;
  const float seconds = to - from;
  for (Foe& foe : foes_) {
    const Monster& spec = foe.spec;
    if (spec.dies() > 0.0f && to >= spec.dies()) {
      foe.fallen = true;
    }
    foe.motion = protocol::MOTION_IDLE;
    if (foe.pushed_at >= 0.0f && to < foe.pushed_at + kPushTime) {
      const float share = (to - foe.pushed_at) / kPushTime;
      foe.at = Vector2Lerp(foe.pushed_from, foe.pushed_to,
                           1.0f - (1.0f - share) * (1.0f - share));
      continue;
    }
    if (foe.fallen) {
      continue;
    }
    if (!spec.legs().empty()) {
      const Spot spot = Walked(spec.at(), Radians(spec.yaw()), spec.legs(), to);
      foe.at = spot.at, foe.yaw = spot.yaw, foe.motion = spot.motion;
    }
    int chased = -1;
    for (int p = 0; p < shot_.players_size(); ++p) {
      if (shot_.players(p).id() == spec.charge() && to >= spec.charge_at()) {
        chased = p;
      }
      if (shot_.players(p).id() == spec.face() && spec.legs().empty()) {
        const Vector2 at = PlayerAt(p, to);
        foe.yaw = std::atan2(at.x - foe.at.x, at.y - foe.at.y);
      }
    }
    if (chased >= 0) {
      const Vector2 at = PlayerAt(chased, to);
      const Vector2 way = Vector2Subtract(at, foe.at);
      const float distance = Vector2Length(way);
      foe.yaw = std::atan2(way.x, way.y);
      const auto kind = content_.monsters.kinds.find(spec.kind());
      const float reach = kind != content_.monsters.kinds.end()
                              ? kind->second.fight().reach()
                              : 1.0f;
      if (distance > reach) {
        const float step = std::min(kRunSpeed * seconds, distance - reach);
        foe.at = Vector2Add(foe.at, Vector2Scale(way, step / distance));
        foe.motion = protocol::MOTION_RUN;
      } else {
        foe.motion = protocol::MOTION_ATTACK;
      }
    }
  }
  // Those standing don't stand in each other.
  constexpr float kApart = 0.75f;
  for (size_t i = 0; i < foes_.size(); ++i) {
    for (size_t j = i + 1; j < foes_.size(); ++j) {
      Foe& a = foes_[i];
      Foe& b = foes_[j];
      if (a.fallen || b.fallen) {
        continue;
      }
      const Vector2 way = Vector2Subtract(b.at, a.at);
      const float distance = Vector2Length(way);
      if (distance < kApart && distance > 1e-4f) {
        const Vector2 half =
            Vector2Scale(way, 0.5f * (kApart - distance) / distance);
        a.at = Vector2Subtract(a.at, half);
        b.at = Vector2Add(b.at, half);
      }
    }
  }
}

protocol::World Stage::At(float game) {
  // In small steps, so that charges stop in reach and blows land in order.
  constexpr float kStep = 1.0f / 120.0f;
  while (now_ < game) {
    const float to = std::min(game, now_ + kStep);
    Step(now_, to);
    now_ = to;
  }

  protocol::World world;
  const int land = LandOf(shot_);
  world.set_seed(kWorldSeed + kLands[land].seed);
  world.set_land(land);
  for (int i = 0; i < shot_.players_size(); ++i) {
    const Player& who = shot_.players(i);
    Spot spot = Walked(who.at(), Radians(who.yaw()), who.legs(), game);
    protocol::Player* player = world.add_players();
    player->set_id(who.id());
    player->set_name(who.name());
    const uint32_t level = who.level() > 0 ? who.level() : 10;
    player->set_level(level);
    // As the server's levels have it.
    player->set_max_health(100 + 20 * (level - 1));
    player->set_health(player->max_health());
    player->set_weapon(who.weapon());
    for (const uint32_t item : who.wearing()) {
      player->add_wearing(item);
    }
    const auto facing = [&](uint32_t id) {
      for (const Foe& foe : foes_) {
        if (foe.spec.id() == id) {
          spot.yaw = std::atan2(foe.at.x - spot.at.x, foe.at.y - spot.at.y);
        }
      }
    };
    if (spot.motion == protocol::MOTION_IDLE && who.face() != 0) {
      facing(who.face());
    }
    // The last skill they started, while it lasts.
    uint32_t casts = 0;
    for (const Cast& cast : who.casts()) {
      if (cast.at() > game) {
        continue;
      }
      ++casts;
      if (game < cast.at() + BlowOf(who, cast).lasts) {
        spot.motion = protocol::MOTION_SKILL;
        player->set_skill(cast.skill());
        player->set_target(cast.target());
        facing(cast.target());
      }
    }
    player->set_casts(casts);
    player->set_x(spot.at.x);
    player->set_z(spot.at.y);
    player->set_yaw(spot.yaw);
    player->set_motion(spot.motion);
  }
  if (shot_.players_size() > 0) {
    world.set_you(shot_.players(0).id());
  }
  for (const Foe& foe : foes_) {
    protocol::Monster* monster = world.add_monsters();
    monster->set_id(foe.spec.id());
    monster->set_kind(foe.spec.kind());
    monster->set_level(foe.spec.level() > 0 ? foe.spec.level() : 5);
    const auto kind = content_.monsters.kinds.find(foe.spec.kind());
    const uint32_t health =
        kind != content_.monsters.kinds.end() ? kind->second.max_health() : 80;
    monster->set_max_health(health);
    monster->set_health(foe.fallen ? 0 : health);
    monster->set_x(foe.at.x);
    monster->set_z(foe.at.y);
    monster->set_yaw(foe.yaw);
    monster->set_motion(foe.fallen ? protocol::MOTION_DEAD : foe.motion);
  }
  return world;
}

}  // namespace nanommo::cinema
