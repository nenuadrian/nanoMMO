#ifndef NANOMMO_CLIENT_SEA_H_
#define NANOMMO_CLIENT_SEA_H_

#include <raylib.h>

#include "client/atmosphere.h"
#include "terrain/terrain.h"

namespace nanommo {

/// The sea around a land, in the land's look, which moves on its own.
class Sea {
 public:
  Sea(const Terrain& terrain, const Look& look);
  ~Sea();

  Sea(const Sea&) = delete;
  Sea& operator=(const Sea&) = delete;

  /// Draws it as it is a number of seconds in, lit like the world, with the
  /// sun's light in bands, or smooth with no bands.
  void Draw(const Camera3D& camera, const Atmosphere& atmosphere, float bands,
            float seconds) const;

  int triangles() const { return model_.meshes[0].triangleCount; }

 private:
  Shader shader_;
  Model model_;
  /// The ground's height at each corner of its cells.
  Texture2D ground_{};
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_SEA_H_
