#include "client/gui.h"

#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

#include "client/theme.h"

namespace nanommo {

Gui::Gui() {
  // raylib already scales for high-DPI screens, so the fonts need no scaling
  // for them. Our scale is for the screen's size.
  rlImGuiSetLoadFontsCallback([] { LoadFonts(FontFolder()); });
  rlImGuiSetup(true);
  ImGui::GetIO().IniFilename = nullptr;
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  // The cursor is ours, as the world has cursors of its own (see cursor.h).
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
}

Gui::~Gui() { rlImGuiShutdown(); }

void Gui::Begin(float player_scale) {
  // In points, as ImGui lays out in, so a Retina screen counts as half its
  // pixels.
  const float scale =
      InterfaceScale(static_cast<float>(GetScreenHeight()), player_scale);
  if (scale != applied_) {
    ApplyTheme(scale);
    applied_ = scale;
  }
  rlImGuiBegin();
}

void Gui::End() { rlImGuiEnd(); }

}  // namespace nanommo
