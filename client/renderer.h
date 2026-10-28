#ifndef NANOMMO_CLIENT_RENDERER_H_
#define NANOMMO_CLIENT_RENDERER_H_

#include <raylib.h>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "client/animator.h"
#include "client/atmosphere.h"
#include "client/camera.h"
#include "client/characters.h"
#include "client/chunks.h"
#include "client/cube.h"
#include "client/outline.h"
#include "client/sea.h"
#include "client/trees.h"
#include "content/content.h"
#include "terrain/terrain.h"

namespace nanommo {

class Renderer {
 public:
  explicit Renderer(const Content& content = Content());
  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  void Draw() const;

  /// Draws one figure on a plain background, for previews in the UI.
  void DrawPreview(const Figure& figure, const Camera3D& camera) const;

  void Clear() const;

  /// The sky's colour.
  void set_background(const std::array<float, 3>& background) {
    atmosphere_.sky = background;
  }

  /// For the settings window, which edits the atmosphere in place.
  Atmosphere* mutable_atmosphere() { return &atmosphere_; }

  const Camera3D& camera() const { return camera_; }
  void set_camera(const Camera3D& camera) { camera_ = camera; }

  /// Cel shading's bands and outlines, on or off. It starts off.
  void set_cel_shading(bool on);

  /// Builds the terrain's meshes in the look's colours, in place of any we
  /// had, and takes on the look's atmosphere.
  void set_terrain(const Terrain& terrain, const Look& look);

  void set_cubes(std::vector<Cube> cubes) { cubes_ = std::move(cubes); }

  /// What the last Draw asked the GPU for: the terrain, the trees and the
  /// sea.
  struct Work {
    int draws = 0;
    int triangles = 0;
  };
  const Work& work() const { return work_; }

  const std::vector<Figure>& figures() const { return figures_; }
  void set_figures(std::vector<Figure> figures) {
    figures_ = std::move(figures);
  }

 private:
  void UnloadTerrain();
  void DrawCounted(const Mesh& mesh, const Material& material,
                   const Matrix& transform) const;
  void DrawFigure(const Figure& figure) const;

  Atmosphere atmosphere_ = kMeadows.atmosphere;
  Camera3D camera_ = GameCamera();
  std::vector<Cube> cubes_;
  std::vector<Figure> figures_;
  // Draw is const, as drawing doesn't change what we draw, but it counts.
  mutable Work work_;

  Shader cube_shader_;
  Shader character_shader_;
  Outline outline_;
  bool cel_shading_ = false;
  Model cube_;
  /// Every NPC is this ball, scaled to its radius and tinted its colour.
  Model sphere_;
  std::vector<Face> tree_;
  std::vector<Chunk> chunks_;
  std::unique_ptr<Sea> sea_;
  /// The land, for the shadows to lie on.
  std::optional<Terrain> terrain_;
  Characters characters_;
  /// Each kind of monster, by id, as its model at its height.
  std::map<uint32_t, std::unique_ptr<Characters>> monsters_;
  /// The glows' colours, by the id of the skill each goes with.
  std::map<uint32_t, Color> glows_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_RENDERER_H_
