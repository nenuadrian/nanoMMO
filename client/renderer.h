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
#include "client/fx.h"
#include "client/outline.h"
#include "client/reactions.h"
#include "client/sea.h"
#include "client/sky.h"
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

  /// The sky's colour at the horizon, which the fog fades into.
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

  /// Where the figure is in its walk or run, as its model's clips have it:
  /// none, if it's doing neither.
  std::optional<Gait> GaitOf(const Figure& figure) const;

  /// Moves the skills' fx, the sea's waves and the clouds on by the seconds,
  /// as the figures are now. Then the monsters take their blows, and those
  /// that just landed hold their players, and them, still for a moment.
  void Animate(float seconds) {
    fx_->Update(figures_, seconds);
    reactions_.Update(seconds);
    reactions_.Apply(figures_);
    fx_->Hold(figures_);
    clock_ += seconds;
  }

  /// The player hit the monster, both by their ids.
  void Struck(uint32_t monster, uint32_t player) {
    reactions_.Hit(monster, player);
  }

  /// The monster the player picked, by its id, which gets a ring at its feet,
  /// or 0 for none.
  void set_target(uint32_t target) { target_ = target; }

  /// Where the player's walking to by themselves, which gets a ring of its
  /// own, if they are.
  void set_destination(std::optional<Vector3> destination) {
    destination_ = destination;
  }

  /// How far the fog has lifted, as Loading::clear says: from 0, where it
  /// hides all but what's nearest, to 1, the atmosphere's own.
  void set_clear(float clear) { clear_ = clear; }

 private:
  void UnloadTerrain();
  void DrawCounted(const Mesh& mesh, const Material& material,
                   const Matrix& transform) const;
  void DrawFigure(const Figure& figure) const;

  Atmosphere atmosphere_ = kMeadows.atmosphere;
  Camera3D camera_ = GameCamera();
  std::vector<Cube> cubes_;
  std::vector<Figure> figures_;
  uint32_t target_ = 0;
  std::optional<Vector3> destination_;
  float clear_ = 1.0f;
  /// The seconds Animate has moved the world on by, which the sea's waves and
  /// the clouds go by.
  float clock_ = 0.0f;
  // Draw is const, as drawing doesn't change what we draw, but it counts.
  mutable Work work_;

  Shader cube_shader_;
  Shader character_shader_;
  Outline outline_;
  bool cel_shading_ = false;
  Model cube_;
  Model disc_;
  Model spire_;
  std::vector<Face> tree_;
  std::vector<Chunk> chunks_;
  std::unique_ptr<Sea> sea_;
  /// The land, for the shadows to lie on.
  std::optional<Terrain> terrain_;
  Characters characters_;
  /// Each kind of monster, by id, as its model at its height.
  std::map<uint32_t, std::unique_ptr<Characters>> monsters_;
  std::unique_ptr<Fx> fx_;
  Reactions reactions_;
  std::unique_ptr<Sky> sky_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_RENDERER_H_
