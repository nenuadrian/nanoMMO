#include "client/guild_window.h"

#include <imgui.h>

#include "client/theme.h"

namespace nanommo {

const char* RankName(protocol::Rank rank) {
  switch (rank) {
    case protocol::RANK_LEADER:
      return "Leader";
    case protocol::RANK_OFFICER:
      return "Officer";
    default:
      return "Member";
  }
}

std::optional<protocol::GuildAction> GuildWindow::Build(
    const protocol::Guild& yours) {
  if (!visible_) {
    return std::nullopt;
  }
  std::optional<protocol::GuildAction> action;
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x * 0.5f, display.y * 0.4f},
                          ImGuiCond_FirstUseEver, {0.5f, 0.5f});
  if (BeginPanel("Guild", &visible_,
                 ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings)) {
    const float width = Ui(280.0f);
    if (yours.name().empty()) {
      ImGui::TextUnformatted("You're in no guild. Found one:");
      ImGui::TextDisabled("3 to 24 letters, one space between words.");
      ImGui::SetNextItemWidth(width);
      const bool entered = ImGui::InputTextWithHint(
          "##name", "Its name", name_.data(), name_.size(),
          ImGuiInputTextFlags_EnterReturnsTrue);
      const bool named = name_[0] != '\0';
      if ((GameButton("Found the guild", {width, 0.0f}, true, named) ||
           entered) &&
          named) {
        action.emplace().set_create(name_.data());
      }
    } else {
      ImGui::TextUnformatted(yours.name().c_str());
      ImGui::TextDisabled("%s", RankName(yours.rank()));
    }
  }
  EndPanel();
  return action;
}

}  // namespace nanommo
