#include "client/menu.h"

#include <imgui.h>

#include <cstddef>

#include "extras/IconsFontAwesome6.h"

namespace nanommo {

namespace {

struct Preset {
  const char* name;
  std::array<float, 3> color;
};

constexpr std::array<Preset, 5> kPresets = {{
    {"Day", {0.62f, 0.78f, 0.9f}},
    {"Night", {0.07f, 0.08f, 0.12f}},
    {"Dusk", {0.25f, 0.13f, 0.31f}},
    {"Forest", {0.07f, 0.21f, 0.13f}},
    {"Ember", {0.36f, 0.06f, 0.05f}},
}};

}  // namespace

bool Menu::Build(Window& window, std::array<float, 3>& background) {
  bool log_out = false;
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("Game")) {
      log_out = ImGui::MenuItem(ICON_FA_RIGHT_FROM_BRACKET " Log out");
      if (ImGui::MenuItem(ICON_FA_POWER_OFF " Quit")) {
        window.Close();
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      ImGui::MenuItem(ICON_FA_GEAR " Settings", nullptr, &show_settings_);
      ImGui::MenuItem(ICON_FA_SHAPES " ImGui demo", nullptr, &show_demo_);
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

  if (show_settings_) {
    ImGui::Begin("Settings", &show_settings_,
                 ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::ColorEdit3("Background", background.data());
    for (std::size_t i = 0; i < kPresets.size(); ++i) {
      if (i > 0) {
        ImGui::SameLine();
      }
      if (ImGui::Button(kPresets[i].name)) {
        background = kPresets[i].color;
      }
    }
    ImGui::End();
  }

  if (show_demo_) {
    ImGui::ShowDemoWindow(&show_demo_);
  }
  return log_out;
}

}  // namespace nanommo
