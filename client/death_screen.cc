#include "client/death_screen.h"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <string>

namespace nanommo {

namespace {

// A frame that took long counts as a short one, as for the loading screen.
constexpr float kLongestFrame = 1.0f / 30.0f;

constexpr ImVec2 kButton{220.0f, 0.0f};

}  // namespace

void DeathScreen::Update(float seconds, bool dead) {
  const float time = std::min(seconds, kLongestFrame);
  if (dead) {
    dark_ = std::min(dark_ + time * kDim / kDarken, kDim);
  } else if (was_dead_) {
    // Back in the middle: from all dark, so that the jump there is hidden.
    dark_ = 1.0f;
  } else {
    dark_ = std::max(dark_ - time / kReturn, 0.0f);
  }
  was_dead_ = dead;
}

bool DeathScreen::Build(bool dead, uint32_t wait_ms) const {
  if (dark_ > 0.0f) {
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        {0.0f, 0.0f}, ImGui::GetIO().DisplaySize,
        IM_COL32(0, 0, 0, static_cast<int>(dark_ * 255.0f)));
  }
  if (!dead) {
    return false;
  }
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({screen.x / 2.0f, screen.y * 0.4f}, ImGuiCond_Always,
                          {0.5f, 0.5f});
  ImGui::Begin("Dead", nullptr,
               ImGuiWindowFlags_NoDecoration |
                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings);
  const char* title = "You died";
  ImGui::PushFont(nullptr, ImGui::GetFontSize() * 2.4f);
  ImGui::SetCursorPosX((kButton.x - ImGui::CalcTextSize(title).x) / 2.0f +
                       ImGui::GetStyle().WindowPadding.x);
  ImGui::TextUnformatted(title);
  ImGui::PopFont();
  ImGui::Spacing();
  // Whole seconds, rounded up, so that it says 1 until it's time.
  const uint32_t seconds = (wait_ms + 999) / 1000;
  const bool ready = wait_ms == 0;
  const std::string label =
      ready ? "Respawn" : "Respawn in " + std::to_string(seconds);
  ImGui::BeginDisabled(!ready);
  const bool clicked = ImGui::Button(label.c_str(), kButton);
  ImGui::EndDisabled();
  ImGui::End();
  return clicked && ready;
}

}  // namespace nanommo
