#ifndef NANOMMO_CLIENT_RENDERER_H_
#define NANOMMO_CLIENT_RENDERER_H_

#include <raylib.h>

#include <array>
#include <utility>
#include <vector>

#include "client/animator.h"
#include "client/characters.h"
#include "client/cube.h"

namespace nanommo {

class Renderer {
 public:
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

  void set_cubes(std::vector<Cube> cubes) { cubes_ = std::move(cubes); }

  const std::vector<Figure>& figures() const { return figures_; }
  void set_figures(std::vector<Figure> figures) {
    figures_ = std::move(figures);
  }

 private:
  std::array<float, 3> background_ = {0.07f, 0.08f, 0.12f};
  std::vector<Cube> cubes_;
  std::vector<Figure> figures_;

  Shader cube_shader_;
  Shader character_shader_;
  Model cube_;
  Characters characters_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_RENDERER_H_
