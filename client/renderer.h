#ifndef NANOMMO_CLIENT_RENDERER_H_
#define NANOMMO_CLIENT_RENDERER_H_

#include <raylib.h>

#include <array>
#include <utility>
#include <vector>

#include "client/cube.h"

namespace nanommo {

class Renderer {
 public:
  Renderer();
  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  void Draw() const;

  const std::array<float, 3>& background() const { return background_; }
  void set_background(const std::array<float, 3>& background) {
    background_ = background;
  }
  /// For the settings window, which edits the colour in place.
  std::array<float, 3>* mutable_background() { return &background_; }

  void set_cubes(std::vector<Cube> cubes) { cubes_ = std::move(cubes); }

 private:
  std::array<float, 3> background_ = {0.07f, 0.08f, 0.12f};
  std::vector<Cube> cubes_;

  Shader shader_;
  Model cube_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_RENDERER_H_
