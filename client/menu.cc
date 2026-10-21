#include "client/menu.h"

#include <imgui.h>

#include <array>
#include <cmath>
#include <cstddef>

#include "extras/IconsFontAwesome6.h"

namespace nanommo {

namespace {

struct Preset {
  const char* name;
  Atmosphere atmosphere;
};

constexpr std::array<Preset, 4> kPresets = {{
    {"Dusk",
     {.sky = {0.78f, 0.5f, 0.42f},
      .fog_near = 40.0f,
      .fog_far = 140.0f,
      .sun = {-0.8f, 0.3f, 0.2f},
      .sunlight = {1.0f, 0.6f, 0.35f},
      .ambient = {0.3f, 0.28f, 0.42f}}},
    {"Night",
     {.sky = {0.04f, 0.06f, 0.12f},
      .fog_near = 20.0f,
      .fog_far = 100.0f,
      .sun = {0.4f, 0.8f, -0.3f},
      .sunlight = {0.25f, 0.3f, 0.45f},
      .ambient = {0.1f, 0.12f, 0.2f}}},
    {"Mist",
     {.sky = {0.72f, 0.75f, 0.76f},
      .fog_near = 5.0f,
      .fog_far = 60.0f,
      .sun = {0.3f, 1.0f, 0.5f},
      .sunlight = {0.25f, 0.25f, 0.25f},
      .ambient = {0.68f, 0.7f, 0.72f}}},
    {"Ash",
     {.sky = {0.46f, 0.22f, 0.13f},
      .fog_near = 35.0f,
      .fog_far = 130.0f,
      .sun = {0.6f, 0.45f, -0.4f},
      .sunlight = {1.0f, 0.55f, 0.3f},
      .ambient = {0.36f, 0.28f, 0.28f}}},
}};

// A slider from 0 to 100 percent, for a setting from nothing to full.
void SliderPercent(const char* label, float& value) {
  int percent = static_cast<int>(std::lround(value * 100.0f));
  // Ctrl and a click lets a player type a number, which ImGui takes past the
  // slider's ends unless it's told to clamp it, and 500% would be far too loud.
  if (ImGui::SliderInt(label, &percent, 0, 100, "%d%%",
                       ImGuiSliderFlags_AlwaysClamp)) {
    value = static_cast<float>(percent) / 100.0f;
  }
}

}  // namespace

bool Menu::Build(Window& window, Settings& settings, Atmosphere& atmosphere,
                 const Atmosphere& land, bool* show_character) {
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
      ImGui::MenuItem(ICON_FA_CIRCLE_USER " Character", "C", show_character);
      ImGui::MenuItem(ICON_FA_GEAR " Settings", nullptr, &show_settings_);
      ImGui::MenuItem(ICON_FA_SHAPES " ImGui demo", nullptr, &show_demo_);
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

  if (show_settings_) {
    ImGui::Begin("Settings", &show_settings_,
                 ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SeparatorText(ICON_FA_VOLUME_HIGH " Sound");
    SliderPercent("Volume", settings.sound.master);
    SliderPercent("Music", settings.sound.music);
    ImGui::Checkbox("Mute", &settings.sound.muted);
    ImGui::TextDisabled(ICON_FA_MUSIC
                        " \"Background Music\" by Pavel Bekirov, from Pixabay");
    ImGui::SeparatorText(ICON_FA_DISPLAY " Graphics");
    ImGui::Checkbox("Bloom", &settings.graphics.bloom);
    ImGui::Checkbox("Cel shading", &settings.graphics.cel_shading);
    ImGui::SeparatorText(ICON_FA_CLOUD_SUN " Sky");
    ImGui::ColorEdit3("Sky", atmosphere.sky.data());
    ImGui::DragFloatRange2("Fog", &atmosphere.fog_near, &atmosphere.fog_far,
                           1.0f, 0.0f, 400.0f, "%.0f m");
    ImGui::SliderFloat3("Sun", atmosphere.sun.data(), -1.0f, 1.0f);
    ImGui::ColorEdit3("Sunlight", atmosphere.sunlight.data());
    ImGui::ColorEdit3("Ambient", atmosphere.ambient.data());
    for (std::size_t i = 0; i < kPresets.size(); ++i) {
      if (i > 0) {
        ImGui::SameLine();
      }
      if (ImGui::Button(kPresets[i].name)) {
        atmosphere = kPresets[i].atmosphere;
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ROTATE_LEFT " Reset")) {
      atmosphere = land;
    }
    ImGui::End();
  }

  if (show_demo_) {
    ImGui::ShowDemoWindow(&show_demo_);
  }
  return log_out;
}

}  // namespace nanommo
