#include "client/duel_window.h"

#include <imgui.h>

#include <cfloat>
#include <cstdint>
#include <optional>
#include <string>

#include "client/theme.h"

namespace nanommo {

uint32_t CountdownSeconds(uint32_t milliseconds) {
  return (milliseconds + 999) / 1000;
}

std::optional<protocol::DuelAction> DuelWindow::Build(
    const protocol::Duel& duel) {
  if (duel.state() != state_) {
    answered_ = -kAnswering;
    if (duel.state() == protocol::DUEL_FIGHTING) {
      started_ = ImGui::GetTime();
    }
    state_ = duel.state();
  }
  // Big, in gold, over the world and under the windows, as "You died" is.
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  const auto shout = [&screen](const std::string& words, float alpha) {
    ImFont* title = GetFonts().title;
    const float size = Ui(110.0f);
    const ImVec2 extent =
        title->CalcTextSizeA(size, FLT_MAX, 0.0f, words.c_str());
    DrawGradientText(ImGui::GetBackgroundDrawList(), title, size,
                     {(screen.x - extent.x) / 2.0f, screen.y * 0.22f},
                     Faded(colors::kGoldLight, alpha),
                     Faded(colors::kGoldDark, alpha), words.c_str());
  };
  const double fighting = ImGui::GetTime() - started_;
  if (duel.state() == protocol::DUEL_COUNTDOWN) {
    shout(std::to_string(CountdownSeconds(duel.countdown())), 1.0f);
  } else if (duel.state() == protocol::DUEL_FIGHTING && fighting < kFight) {
    shout("Fight!", static_cast<float>(1.0 - fighting / kFight));
  }
  if (duel.state() != protocol::DUEL_ASKED ||
      ImGui::GetTime() - answered_ < kAnswering) {
    return std::nullopt;
  }
  ImGui::SetNextWindowPos({screen.x / 2.0f, screen.y * 0.3f}, ImGuiCond_Always,
                          {0.5f, 0.5f});
  bool open = true;
  std::optional<protocol::DuelAction> action;
  BeginPanel("Duel?", &open,
             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings);
  ImGui::Text("%s asks you to duel.", duel.opponent().c_str());
  ImGui::Spacing();
  if (GameButton("Duel", {Ui(150.0f), 0.0f}, true)) {
    action.emplace().set_accept(true);
  }
  ImGui::SameLine();
  // Closing it says no too, so that someone else can ask either of you.
  if (GameButton("Not now", {Ui(150.0f), 0.0f}) || !open) {
    action.emplace().set_decline(true);
  }
  EndPanel();
  if (action) {
    answered_ = ImGui::GetTime();
  }
  return action;
}

}  // namespace nanommo
