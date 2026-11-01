#include "cinema/film.h"

#include <raymath.h>

#include <algorithm>
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
    if (shot.camera().empty()) {
      return absl::InvalidArgumentError(
          absl::StrCat(shot.id(), ": no camera keys"));
    }
  }
  return film;
}

int LandOf(const Shot& shot) { return LandNamed(shot.land()); }

Terrain TerrainOf(int land) {
  return Terrain(kWorldSeed + kLands[land].seed, kLands[land].wading);
}

Vector3 Place(const Point& point, const Terrain& terrain) {
  const float base =
      point.sea() ? Terrain::kSeaLevel : terrain.Height(point.x(), point.z());
  return {point.x(), base + point.up(), point.z()};
}

Camera3D CameraAt(const Shot& shot, float seconds, const Terrain& terrain) {
  return Along(shot.camera(), seconds, shot.ease(), terrain);
}

}  // namespace nanommo::cinema
