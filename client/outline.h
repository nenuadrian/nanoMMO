#ifndef NANOMMO_CLIENT_OUTLINE_H_
#define NANOMMO_CLIENT_OUTLINE_H_

#include <raylib.h>

namespace nanommo {

/// Cel shading's outlines, the way raylib's example draws them: a model is
/// drawn a second time, a little bigger and inside out, in a dark colour, so
/// that it shows around the model's edges. There's a shader for models without
/// bones, and one for models with them.
struct Outline {
  Shader still;
  Shader skinned;
};

/// Draws a model with an outline's shader, inside out: with its front faces
/// culled, its back faces show, behind the model.
void DrawOutline(const Model& model, const Matrix& transform, Shader shader);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_OUTLINE_H_
