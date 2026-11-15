#include "client/glsl.h"

#include <raylib.h>

#include <optional>
#include <string>
#include <string_view>

namespace nanommo {
namespace {

// The source as the GPU takes it: on a computer, as it is.
std::optional<std::string> ForThisGpu(const char* source) {
  if (source == nullptr) {
    return std::nullopt;
  }
  std::string glsl(source);
#if defined(__EMSCRIPTEN__)
  constexpr std::string_view kDesktop = "#version 330";
  if (glsl.starts_with(kDesktop)) {
    glsl.replace(0, kDesktop.size(),
                 "#version 300 es\n"
                 "precision highp float;\n"
                 "precision highp int;");
  }
#endif
  return glsl;
}

}  // namespace

Shader LoadGlsl(const char* vertex, const char* fragment) {
  const std::optional<std::string> vs = ForThisGpu(vertex);
  const std::optional<std::string> fs = ForThisGpu(fragment);
  return LoadShaderFromMemory(vs ? vs->c_str() : nullptr,
                              fs ? fs->c_str() : nullptr);
}

}  // namespace nanommo
