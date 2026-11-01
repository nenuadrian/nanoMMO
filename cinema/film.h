#ifndef NANOMMO_CINEMA_FILM_H_
#define NANOMMO_CINEMA_FILM_H_

#include <raylib.h>

#include <string>

#include "absl/status/statusor.h"
#include "cinema/film.pb.h"
#include "terrain/terrain.h"

namespace nanommo::cinema {

/// The shots in a text file, as the content files are written.
absl::StatusOr<Film> ReadFilm(const std::string& path);

/// The land a shot is in, by its place in kLands.
int LandOf(const Shot& shot);

/// The land's terrain, as the server builds it for the first world.
Terrain TerrainOf(int land);

/// The point in the land, with its height worked out from the ground or the
/// sea.
Vector3 Place(const Point& point, const Terrain& terrain);

/// Where the camera is at that second of the shot, and what it looks at.
Camera3D CameraAt(const Shot& shot, float seconds, const Terrain& terrain);

}  // namespace nanommo::cinema

#endif  // NANOMMO_CINEMA_FILM_H_
