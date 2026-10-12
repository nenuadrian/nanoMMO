#ifndef NANOMMO_CLIENT_RENDERER_H_
#define NANOMMO_CLIENT_RENDERER_H_

#include <raylib.h>

#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "client/animator.h"
#include "client/camera.h"
#include "client/characters.h"
#include "client/cube.h"
#include "client/trees.h"
#include "terrain/terrain.h"

namespace nanommo {

class Renderer {
 public:
  /// In metres from the camera: the fog starts at the first, and hides
  /// everything past the second.
  static constexpr float kFogNear = 50.0f;
  static constexpr float kFogFar = 140.0f;

  Renderer();
  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  void Draw() const;

  void Clear() const;

  const std::array<float, 3>& background() const { return background_; }
  void set_background(const std::array<float, 3>& background) {
    background_ = background;
  }
  /// For the settings window, which edits the colour in place.
  std::array<float, 3>* mutable_background() { return &background_; }

  const Camera3D& camera() const { return camera_; }
  void set_camera(const Camera3D& camera) { camera_ = camera; }

  /// Builds the terrain's meshes, in place of any we had.
  void set_terrain(const Terrain& terrain);

  void set_cubes(std::vector<Cube> cubes) { cubes_ = std::move(cubes); }

  const std::vector<Figure>& figures() const { return figures_; }
  void set_figures(std::vector<Figure> figures) {
    figures_ = std::move(figures);
  }

 private:
  void UnloadTerrain();

  std::array<float, 3> background_ = {0.62f, 0.78f, 0.9f};
  Camera3D camera_ = GameCamera();
  std::vector<Cube> cubes_;
  std::vector<Figure> figures_;

  Shader cube_shader_;
  Shader character_shader_;
  Model cube_;
  std::optional<Model> ground_;
  std::optional<Model> sea_;
  std::optional<Model> tree_;
  std::vector<Matrix> trees_;
  Characters characters_;
  Characters monsters_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_RENDERER_H_
