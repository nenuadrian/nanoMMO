#include "client/gui.h"

#include <imgui.h>
#include <rlImGui.h>

namespace nanommo {

Gui::Gui() {
  // raylib already scales for high-DPI screens, so the font needs no scaling.
  rlImGuiSetLoadFontsCallback([] {
    ImFontConfig config;
    // rlImGui merges its icons in, which needs a font with its size set.
    config.SizePixels = 13.0f;
    config.PixelSnapH = true;
    ImGui::GetIO().Fonts->AddFontDefaultVector(&config);
  });
  rlImGuiSetup(true);
  ImGui::GetIO().IniFilename = nullptr;
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
}

Gui::~Gui() { rlImGuiShutdown(); }

void Gui::Begin() { rlImGuiBegin(); }

void Gui::End() { rlImGuiEnd(); }

}  // namespace nanommo
