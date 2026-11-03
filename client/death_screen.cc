#include "client/death_screen.h"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <string>

#include "client/theme.h"

namespace nanommo {

namespace {

// A frame that took long counts as a short one, as for the loading screen.
constexpr float kLongestFrame = 1.0f / 30.0f;

constexpr ImVec2 kButton{260.0f, 50.0f};

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
  ImDrawList* draw = ImGui::GetBackgroundDrawList();
  // A red that comes in from the edges.
  const float edge = std::min(screen.x, screen.y) * 0.3f;
  const ImU32 red = Faded(colors::kWine, 0.7f);
  const ImU32 clear = Faded(colors::kWine, 0.0f);
  draw->AddRectFilledMultiColor({0.0f, 0.0f}, {edge, screen.y}, red, clear,
                                clear, red);
  draw->AddRectFilledMultiColor({screen.x - edge, 0.0f}, screen, clear, red,
                                red, clear);
  ImFont* title = GetFonts().title;
  const char* words = "You died";
  const float size = Ui(72.0f);
  const ImVec2 extent = title->CalcTextSizeA(size, FLT_MAX, 0.0f, words);
  const float top = screen.y * 0.2f;
  DrawGradientText(draw, title, size, {(screen.x - extent.x) / 2.0f, top},
                   IM_COL32(255, 120, 100, 255), colors::kCrimson, words);
  DrawDivider(draw, {screen.x / 2.0f - extent.x * 0.6f, top + extent.y},
              extent.x * 1.2f);

  ImGui::SetNextWindowPos({screen.x / 2.0f, top + extent.y + Ui(30.0f)},
                          ImGuiCond_Always, {0.5f, 0.0f});
  ImGui::Begin("Dead", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings);
  // Whole seconds, rounded up, so that it says 1 until it's time.
  const uint32_t seconds = (wait_ms + 999) / 1000;
  const bool ready = wait_ms == 0;
  const std::string label =
      ready ? "Respawn" : "Respawn in " + std::to_string(seconds);
  const bool clicked = GameButton(label.c_str(), Ui(kButton), true, ready);
  ImGui::End();
  return clicked && ready;
}

}  // namespace nanommo
