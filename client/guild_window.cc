#include "client/guild_window.h"

#include <imgui.h>

#include <algorithm>
#include <utility>

#include "client/theme.h"

namespace nanommo {

namespace {

// As long as the server keeps an invitation.
constexpr double kInvitationLife = 60.0;

}  // namespace

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

void GuildWindow::Invited(protocol::GuildInvite invite) {
  invite_ = std::move(invite);
  invited_at_ = ImGui::GetTime();
}

std::optional<protocol::GuildAction> GuildWindow::Build(
    const protocol::Guild& yours) {
  std::optional<protocol::GuildAction> action = BuildQuestion();
  if (!visible_) {
    return action;
  }
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x * 0.5f, display.y * 0.4f},
                          ImGuiCond_FirstUseEver, {0.5f, 0.5f});
  if (BeginPanel("Guild", &visible_,
                 ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings)) {
    const float width = Ui(280.0f);
    const bool named = name_[0] != '\0';
    if (yours.name().empty()) {
      ImGui::TextUnformatted("You're in no guild. Found one:");
      ImGui::TextDisabled("3 to 24 letters, one space between words.");
      ImGui::SetNextItemWidth(width);
      const bool entered = ImGui::InputTextWithHint(
          "##name", "Its name", name_.data(), name_.size(),
          ImGuiInputTextFlags_EnterReturnsTrue);
      if ((GameButton("Found the guild", {width, 0.0f}, true, named) ||
           entered) &&
          named) {
        action.emplace().set_create(name_.data());
      }
    } else {
      ImGui::TextUnformatted(yours.name().c_str());
      ImGui::TextDisabled("%s", RankName(yours.rank()));
      // The roster, the highest rank first, and those playing in white.
      if (ImGui::BeginTable(
              "Roster", 3,
              ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp,
              {width, 0.0f})) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Rank");
        ImGui::TableSetupColumn("Level");
        ImGui::TableHeadersRow();
        for (const protocol::GuildMember& member : yours.members()) {
          ImGui::TableNextRow();
          ImGui::BeginDisabled(!member.online());
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(member.name().c_str());
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(RankName(member.rank()));
          ImGui::TableNextColumn();
          ImGui::Text("%u", member.level());
          ImGui::EndDisabled();
        }
        ImGui::EndTable();
      }
      const auto online = std::ranges::count_if(
          yours.members(),
          [](const protocol::GuildMember& member) { return member.online(); });
      ImGui::TextDisabled("%d of %d playing now.", static_cast<int>(online),
                          yours.members_size());
      // Officers and the leader ask players in, by name.
      if (yours.rank() != protocol::RANK_MEMBER) {
        ImGui::Spacing();
        ImGui::SetNextItemWidth(width);
        const bool entered = ImGui::InputTextWithHint(
            "##player", "Someone playing here", player_.data(), player_.size(),
            ImGuiInputTextFlags_EnterReturnsTrue);
        const bool someone = player_[0] != '\0';
        if ((GameButton("Ask them in", {width, 0.0f}, false, someone) ||
             entered) &&
            someone) {
          action.emplace().set_invite(player_.data());
          player_.fill('\0');
        }
      }
      ImGui::Spacing();
      if (GameButton("Leave the guild", {width, 0.0f})) {
        action.emplace().set_leave(true);
      }
    }
  }
  EndPanel();
  return action;
}

std::optional<protocol::GuildAction> GuildWindow::BuildQuestion() {
  if (!invite_ || ImGui::GetTime() - invited_at_ > kInvitationLife) {
    invite_.reset();
    return std::nullopt;
  }
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x / 2.0f, display.y * 0.3f},
                          ImGuiCond_Always, {0.5f, 0.5f});
  bool open = true;
  std::optional<protocol::GuildAction> action;
  BeginPanel("Join a guild?", &open,
             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings);
  ImGui::Text("%s asks you to join %s.", invite_->from().c_str(),
              invite_->guild().c_str());
  ImGui::Spacing();
  if (GameButton("Join", {Ui(150.0f), 0.0f}, true)) {
    action.emplace().set_accept(true);
    open = false;
  }
  ImGui::SameLine();
  // Saying no tells the server, so that another guild can ask.
  if (GameButton("Not now", {Ui(150.0f), 0.0f})) {
    action.emplace().set_decline(true);
    open = false;
  }
  EndPanel();
  if (!open) {
    invite_.reset();
  }
  return action;
}

}  // namespace nanommo
