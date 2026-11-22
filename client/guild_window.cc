#include "client/guild_window.h"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
#include "client/theme.h"
#include "client/translations.h"

namespace nanommo {

namespace {

// As long as the server keeps an invitation.
constexpr double kInvitationLife = 60.0;

// As the enemies' names over their heads are.
constexpr ImVec4 kEnemy{1.0f, 0.31f, 0.27f, 1.0f};

}  // namespace

const char* RankName(protocol::Rank rank) {
  switch (rank) {
    case protocol::RANK_LEADER:
      return Tr("Leader");
    case protocol::RANK_OFFICER:
      return Tr("Officer");
    default:
      return Tr("Member");
  }
}

std::string TimeLeft(uint32_t seconds) {
  const uint32_t past = seconds % 60;
  return absl::StrCat(seconds / 60, past < 10 ? ":0" : ":", past);
}

void GuildWindow::Invited(protocol::GuildInvite invite) {
  invite_ = std::move(invite);
  invited_at_ = ImGui::GetTime();
}

std::optional<protocol::GuildAction> GuildWindow::Build(
    const protocol::Guild& yours, uint32_t you) {
  std::optional<protocol::GuildAction> action = BuildQuestion();
  // The news comes as the war starts, or its score moves, so its time left
  // counts down here in between.
  if (yours.war().left() != war_left_) {
    war_left_ = yours.war().left();
    war_news_at_ = ImGui::GetTime();
  }
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
      ImGui::TextUnformatted(Tr("You're in no guild. Found one:"));
      ImGui::TextDisabled("%s",
                          Tr("3 to 24 letters, one space between words."));
      ImGui::SetNextItemWidth(width);
      const bool entered = ImGui::InputTextWithHint(
          "##name", Tr("Its name"), name_.data(), name_.size(),
          ImGuiInputTextFlags_EnterReturnsTrue);
      if ((GameButton("Found the guild", {width, 0.0f}, true, named) ||
           entered) &&
          named) {
        action.emplace().set_create(name_.data());
      }
    } else {
      ImGui::TextUnformatted(yours.name().c_str());
      ImGui::TextDisabled("%s", RankName(yours.rank()));
      // At war: with whom, the score from our side, and how long it has left.
      if (yours.has_war()) {
        const protocol::GuildWar& war = yours.war();
        const auto since =
            static_cast<uint32_t>(ImGui::GetTime() - war_news_at_);
        ImGui::Spacing();
        ImGui::TextColored(
            kEnemy, "%s",
            absl::Substitute(Tr("At war with $0"), war.enemy()).c_str());
        ImGui::TextUnformatted(absl::Substitute(Tr("Kills: $0 to $1"),
                                                war.kills(), war.enemy_kills())
                                   .c_str());
        // Translators: $0 is the time the war has left, as 29:59.
        ImGui::TextDisabled(
            "%s", absl::Substitute(
                      Tr("$0 left"),
                      TimeLeft(war.left() > since ? war.left() - since : 0))
                      .c_str());
        ImGui::Spacing();
      }
      // The roster, the highest rank first, and those playing in white.
      if (ImGui::BeginTable(
              "Roster", 3,
              ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp,
              {width, 0.0f})) {
        ImGui::TableSetupColumn(TrLabel("Name"));
        ImGui::TableSetupColumn(TrLabel("Rank"));
        ImGui::TableSetupColumn(TrLabel("Level"));
        ImGui::TableHeadersRow();
        const bool leader = yours.rank() == protocol::RANK_LEADER;
        for (const protocol::GuildMember& member : yours.members()) {
          ImGui::TableNextRow();
          ImGui::PushID(static_cast<int>(member.id()));
          // Grey for those who aren't playing, but not their menu.
          const ImVec4 color = ImGui::GetStyleColorVec4(
              member.online() ? ImGuiCol_Text : ImGuiCol_TextDisabled);
          ImGui::PushStyleColor(ImGuiCol_Text, color);
          ImGui::TableNextColumn();
          ImGui::Selectable(member.name().c_str(), false,
                            ImGuiSelectableFlags_SpanAllColumns);
          ImGui::PopStyleColor();
          if (member.id() != you && yours.rank() > member.rank() &&
              ImGui::BeginPopupContextItem("Member")) {
            if (leader && member.rank() == protocol::RANK_MEMBER &&
                ImGui::MenuItem(TrLabel("Make an officer"))) {
              action.emplace().set_promote(member.id());
            }
            if (leader && member.rank() == protocol::RANK_OFFICER &&
                ImGui::MenuItem(TrLabel("Make a member"))) {
              action.emplace().set_demote(member.id());
            }
            if (ImGui::MenuItem(TrLabel("Remove from the guild"))) {
              action.emplace().set_kick(member.id());
            }
            ImGui::EndPopup();
          }
          ImGui::PushStyleColor(ImGuiCol_Text, color);
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(RankName(member.rank()));
          ImGui::TableNextColumn();
          ImGui::Text("%u", member.level());
          ImGui::PopStyleColor();
          ImGui::PopID();
        }
        ImGui::EndTable();
      }
      const auto online = std::ranges::count_if(
          yours.members(),
          [](const protocol::GuildMember& member) { return member.online(); });
      ImGui::TextDisabled("%s", absl::Substitute(Tr("$0 of $1 playing now."),
                                                 online, yours.members_size())
                                    .c_str());
      // Officers and the leader ask players in, by name.
      if (yours.rank() != protocol::RANK_MEMBER) {
        ImGui::Spacing();
        ImGui::SetNextItemWidth(width);
        const bool entered = ImGui::InputTextWithHint(
            "##player", Tr("Someone playing here"), player_.data(),
            player_.size(), ImGuiInputTextFlags_EnterReturnsTrue);
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
      if (yours.rank() == protocol::RANK_LEADER &&
          GameButton("Disband the guild", {width, 0.0f})) {
        disbanding_ = true;
      }
    }
  }
  EndPanel();
  if (disbanding_ && !yours.name().empty()) {
    if (std::optional<protocol::GuildAction> disband =
            BuildDisband(yours.name())) {
      action = std::move(disband);
    }
  }
  return action;
}

std::optional<protocol::GuildAction> GuildWindow::BuildDisband(
    const std::string& guild) {
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x / 2.0f, display.y * 0.42f},
                          ImGuiCond_Always, {0.5f, 0.5f});
  bool open = true;
  std::optional<protocol::GuildAction> action;
  BeginPanel("Disband?", &open,
             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings);
  ImGui::TextUnformatted(
      absl::Substitute(Tr("Disband $0, for everyone in it, for good?"), guild)
          .c_str());
  ImGui::Spacing();
  if (GameButton("Disband", {Ui(150.0f), 0.0f}, true)) {
    action.emplace().set_disband(true);
    open = false;
  }
  ImGui::SameLine();
  if (GameButton("Keep it", {Ui(150.0f), 0.0f}) ||
      ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    open = false;
  }
  EndPanel();
  disbanding_ = open;
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
  ImGui::TextUnformatted(absl::Substitute(Tr("$0 asks you to join $1."),
                                          invite_->from(), invite_->guild())
                             .c_str());
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
