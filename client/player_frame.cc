#include "client/player_frame.h"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

#include "extras/IconsFontAwesome6.h"

namespace nanommo {

namespace {

constexpr ImVec4 kHealth{0.35f, 0.78f, 0.35f, 1.0f};
constexpr ImVec4 kExperience{0.55f, 0.4f, 0.9f, 1.0f};

void DrawBar(const char* name, int value, int most, ImVec4 color) {
  const float share =
      most > 0 ? std::clamp(static_cast<float>(value) / most, 0.0f, 1.0f)
               : 0.0f;
  const std::string label = std::string(name) + "  " + std::to_string(value) +
                            " / " + std::to_string(most);
  ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
  ImGui::ProgressBar(share, {220.0f, 0.0f}, label.c_str());
  ImGui::PopStyleColor();
}

}  // namespace

void DrawPlayerFrame(const std::vector<Figure>& figures,
                     const protocol::World& world) {
  const auto you = std::ranges::find_if(figures, &Figure::you);
  // Our level comes with our name, once a second, so the frame waits for it.
  if (you == figures.end() || you->level == 0) {
    return;
  }
  constexpr float kMargin = 10.0f;
  ImGui::SetNextWindowPos({kMargin, ImGui::GetFrameHeight() + kMargin});
  ImGui::Begin("You", nullptr,
               ImGuiWindowFlags_NoDecoration |
                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoNav |
                   ImGuiWindowFlags_NoFocusOnAppearing);
  ImGui::Text("%s", you->name.c_str());
  ImGui::SameLine();
  ImGui::TextDisabled("Level %d", you->level);
  DrawBar(ICON_FA_HEART " Health", you->health, you->max_health, kHealth);
  DrawBar(ICON_FA_STAR " Experience", static_cast<int>(world.experience()),
          static_cast<int>(world.next_level()), kExperience);
  ImGui::End();
}

}  // namespace nanommo
