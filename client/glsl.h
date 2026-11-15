#ifndef NANOMMO_CLIENT_GLSL_H_
#define NANOMMO_CLIENT_GLSL_H_

#include <raylib.h>

namespace nanommo {

/// A shader from our GLSL 3.30 sources. WebGL 2 takes GLSL ES 3.00, which
/// has a version line of its own and needs a precision for floats and ints,
/// so in a browser, each source gets those first. A null source is raylib's
/// own, as with LoadShaderFromMemory.
Shader LoadGlsl(const char* vertex, const char* fragment);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_GLSL_H_
