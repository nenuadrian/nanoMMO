#include "client/chat_window.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <deque>
#include <optional>
#include <string>

#include "client/theme.h"
#include "extras/IconsFontAwesome6.h"

namespace nanommo {

namespace {

// In points, at a scale of one.
constexpr float kMargin = 18.0f;
constexpr ImVec2 kSize{440.0f, 240.0f};
constexpr float kButton = 23.0f;

constexpr ImVec4 kName{1.0f, 0.63f, 0.29f, 1.0f};
constexpr ImVec4 kServer{1.0f, 0.83f, 0.3f, 1.0f};

bool Same(const protocol::Chat& a, const protocol::Chat& b) {
  return a.name() == b.name() && a.text() == b.text();
}

}  // namespace

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
    ImGui::SetNextWindowPos({Ui(kMargin), display.y - Ui(kMargin)},
                            ImGuiCond_Always, {0.0f, 1.0f});
    ImGui::SetNextWindowSize(
        {radius * 2.0f + Ui(14.0f), radius * 2.0f + Ui(6.0f)});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin("Chat closed", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();
    const ImVec2 corner = ImGui::GetWindowPos();
    const ImVec2 centre{corner.x + radius, corner.y + Ui(6.0f) + radius};
    ImGui::SetCursorScreenPos({centre.x - radius, centre.y - radius});
    const bool clicked =
        ImGui::InvisibleButton("##open", {radius * 2.0f, radius * 2.0f});
    const bool hovered = ImGui::IsItemHovered();
    if (hovered) {
      ImGui::SetTooltip("Chat (Enter)");
    }
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddCircleFilled({centre.x + Ui(2.0f), centre.y + Ui(3.0f)},
                          radius + Ui(2.0f), Faded(IM_COL32_BLACK, 0.45f));
    draw->AddCircleFilled(centre, radius,
                          hovered ? colors::kCrimson : colors::kWine);
    draw->AddCircle(centre, radius, hovered ? colors::kGold : colors::kBrass, 0,
                    Ui(2.5f));
    ImFont* font = GetFonts().text;
    const float icon = Ui(19.0f);
    const ImVec2 extent =
        font->CalcTextSizeA(icon, FLT_MAX, 0.0f, ICON_FA_COMMENTS);
    DrawShadowedText(draw, font, icon,
                     {centre.x - extent.x / 2.0f, centre.y - extent.y / 2.0f},
                     colors::kGoldLight, ICON_FA_COMMENTS);
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
    // A line with no name is from the server.
    if (line.name().empty()) {
      ImGui::PushStyleColor(ImGuiCol_Text, kServer);
      ImGui::TextWrapped("%s", line.text().c_str());
      ImGui::PopStyleColor();
      continue;
    }
    ImGui::PushFont(GetFonts().bold, 0.0f);
    ImGui::TextColored(kName, "%s:", line.name().c_str());
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextWrapped("%s", line.text().c_str());
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
  if (ImGui::InputTextWithHint("##say", "Press Enter to chat", typing_.data(),
                               typing_.size(),
                               ImGuiInputTextFlags_EnterReturnsTrue) &&
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
