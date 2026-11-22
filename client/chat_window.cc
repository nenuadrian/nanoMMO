#include "client/chat_window.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <string_view>

#include "absl/strings/substitute.h"
#include "client/theme.h"
#include "client/translations.h"
#include "extras/IconsFontAwesome6.h"

namespace nanommo {

namespace {

// In points, at a scale of one.
constexpr float kMargin = 18.0f;
constexpr ImVec2 kSize{440.0f, 240.0f};
constexpr float kButton = 23.0f;
constexpr float kRim = 4.0f;

constexpr ImVec4 kName{1.0f, 0.63f, 0.29f, 1.0f};
constexpr ImVec4 kServer{1.0f, 0.83f, 0.3f, 1.0f};
constexpr ImVec4 kGuild{0.45f, 0.9f, 0.45f, 1.0f};
constexpr ImVec4 kLand{1.0f, 0.95f, 0.77f, 1.0f};

bool Same(const protocol::Chat& a, const protocol::Chat& b) {
  return a.name() == b.name() && a.text() == b.text();
}

}  // namespace

std::optional<std::string> GuildLine(std::string_view typed) {
  constexpr std::string_view kCommand = "/g ";
  if (!typed.starts_with(kCommand)) {
    return std::nullopt;
  }
  return std::string(typed.substr(kCommand.size()));
}

std::optional<std::string> TradeName(std::string_view typed) {
  constexpr std::string_view kCommand = "/trade ";
  if (!typed.starts_with(kCommand)) {
    return std::nullopt;
  }
  return std::string(typed.substr(kCommand.size()));
}

std::optional<std::string> WarName(std::string_view typed) {
  constexpr std::string_view kCommand = "/war ";
  if (!typed.starts_with(kCommand)) {
    return std::nullopt;
  }
  return std::string(typed.substr(kCommand.size()));
}

std::optional<protocol::DuelAction> DuelLine(std::string_view typed) {
  constexpr std::string_view kCommand = "/duel ";
  if (!typed.starts_with(kCommand)) {
    return std::nullopt;
  }
  const std::string_view what = typed.substr(kCommand.size());
  protocol::DuelAction action;
  if (what == "yes") {
    action.set_accept(true);
  } else if (what == "no") {
    action.set_decline(true);
  } else if (what == "yield") {
    action.set_yield(true);
  } else {
    action.set_ask(std::string(what));
  }
  return action;
}

std::size_t ChatWindow::Unread(const std::deque<protocol::Chat>& lines,
                               const std::optional<protocol::Chat>& seen) {
  if (!seen) {
    return lines.size();
  }
  // From the newest back, to the one seen; one that's gone off the top means
  // they're all new.
  for (std::size_t after = 0; after < lines.size(); ++after) {
    if (Same(lines[lines.size() - 1 - after], *seen)) {
      return after;
    }
  }
  return lines.size();
}

std::optional<std::string> ChatWindow::Build(
    const std::deque<protocol::Chat>& lines) {
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  const bool enter = !ImGui::GetIO().WantTextInput &&
                     ImGui::IsKeyPressed(ImGuiKey_Enter, false);
  if (!open_) {
    // A round button in the corner, with how many lines are new on it.
    const float radius = Ui(kButton);
    // Room round the disc for its rim, which the window would cut, as far out
    // as the disc stays in the corner, and above and to the right of it for
    // the count of new lines.
    const float room = Ui(kRim);
    ImGui::SetNextWindowPos(
        {Ui(kMargin) - room, display.y - Ui(kMargin) + room}, ImGuiCond_Always,
        {0.0f, 1.0f});
    ImGui::SetNextWindowSize(
        {room + radius * 2.0f + Ui(14.0f), Ui(6.0f) + radius * 2.0f + room});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin("Chat closed", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();
    const ImVec2 corner = ImGui::GetWindowPos();
    const ImVec2 centre{corner.x + room + radius, corner.y + Ui(6.0f) + radius};
    ImGui::SetCursorScreenPos({centre.x - radius, centre.y - radius});
    const bool clicked =
        ImGui::InvisibleButton("##open", {radius * 2.0f, radius * 2.0f});
    const bool hovered = ImGui::IsItemHovered();
    if (hovered) {
      ImGui::SetTooltip("%s", Tr("Chat (Enter)"));
    }
    ImDrawList* draw = ImGui::GetWindowDrawList();
    DrawRoundButton(draw, centre, radius, ICON_FA_COMMENTS, hovered);
    if (const std::size_t unread = Unread(lines, seen_); unread > 0) {
      const std::string count = unread > 99 ? "99+" : std::to_string(unread);
      ImFont* bold = GetFonts().bold;
      const float size = Ui(13.0f);
      const ImVec2 count_extent =
          bold->CalcTextSizeA(size, FLT_MAX, 0.0f, count.c_str());
      const ImVec2 badge{centre.x + radius * 0.75f, centre.y - radius * 0.75f};
      const float badge_radius =
          std::max(Ui(9.0f), count_extent.x / 2.0f + Ui(4.0f));
      draw->AddCircleFilled(badge, badge_radius, colors::kEmber);
      draw->AddCircle(badge, badge_radius, colors::kNight, 0, Ui(1.5f));
      draw->AddText(
          bold, size,
          {badge.x - count_extent.x / 2.0f, badge.y - count_extent.y / 2.0f},
          colors::kNight, count.c_str());
    }
    ImGui::End();
    if (clicked || enter) {
      open_ = true;
      focus_ = enter;
    }
    return std::nullopt;
  }

  ImGui::SetNextWindowPos({Ui(kMargin), display.y - Ui(kMargin)},
                          ImGuiCond_Always, {0.0f, 1.0f});
  ImGui::SetNextWindowSize(Ui(kSize));
  // Quieter while nobody's using it, so that it doesn't hide the world.
  const bool using_it =
      ImGui::GetIO().WantTextInput ||
      ImGui::IsMouseHoveringRect(
          {Ui(kMargin), display.y - Ui(kMargin) - Ui(kSize.y)},
          {Ui(kMargin) + Ui(kSize.x), display.y - Ui(kMargin)}, false);
  // Space presses the widget that ImGui's keyboard navigation is on, which
  // would open the field again after each line.
  bool open = true;
  BeginPanel("Chat", &open,
             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavInputs |
                 ImGuiWindowFlags_NoFocusOnAppearing |
                 ImGuiWindowFlags_NoSavedSettings,
             using_it ? 0.94f : 0.6f);

  ImGui::BeginChild("Lines", {0.0f, -ImGui::GetFrameHeightWithSpacing()});
  for (const protocol::Chat& line : lines) {
    // What everyone in the land hears, such as a boss's fall, stands out, in
    // bold, in the light gold of a title.
    if (line.channel() == protocol::CHANNEL_LAND) {
      ImGui::PushFont(GetFonts().bold, 0.0f);
      ImGui::PushStyleColor(ImGuiCol_Text, kLand);
      ImGui::TextWrapped("%s", line.text().c_str());
      ImGui::PopStyleColor();
      ImGui::PopFont();
      continue;
    }
    // A line with no name is from the server, and one of those for the guild
    // alone, such as a kill in its war, is green, as its players' lines are.
    if (line.name().empty() && line.channel() == protocol::CHANNEL_GUILD) {
      ImGui::PushStyleColor(ImGuiCol_Text, kGuild);
      ImGui::TextWrapped(
          "%s", absl::Substitute(Tr("[Guild] $0"), line.text()).c_str());
      ImGui::PopStyleColor();
      continue;
    }
    if (line.name().empty()) {
      ImGui::PushStyleColor(ImGuiCol_Text, kServer);
      ImGui::TextWrapped("%s", line.text().c_str());
      ImGui::PopStyleColor();
      continue;
    }
    // The guild's lines are green, all of them, so they stand apart.
    const bool guild = line.channel() == protocol::CHANNEL_GUILD;
    if (guild) {
      ImGui::PushStyleColor(ImGuiCol_Text, kGuild);
    }
    ImGui::PushFont(GetFonts().bold, 0.0f);
    if (guild) {
      ImGui::TextUnformatted(
          absl::Substitute(Tr("[Guild] $0:"), line.name()).c_str());
    } else {
      ImGui::TextColored(kName, "%s:", line.name().c_str());
    }
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextWrapped("%s", line.text().c_str());
    if (guild) {
      ImGui::PopStyleColor();
    }
  }
  // Follow the newest line, unless the player has scrolled up to read.
  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
    ImGui::SetScrollHereY(1.0f);
  }
  ImGui::EndChild();

  if (enter || focus_) {
    ImGui::SetKeyboardFocusHere();
    focus_ = false;
  }
  ImGui::SetNextItemWidth(-FLT_MIN);
  std::optional<std::string> said;
  if (ImGui::InputTextWithHint(
          "##say", Tr("Press Enter to chat, /g for your guild"), typing_.data(),
          typing_.size(), ImGuiInputTextFlags_EnterReturnsTrue) &&
      typing_[0] != '\0') {
    said = typing_.data();
    typing_.fill('\0');
  }
  EndPanel();
  open_ = open;
  if (!lines.empty()) {
    seen_ = lines.back();
  }
  return said;
}

}  // namespace nanommo
