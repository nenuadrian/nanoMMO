#include "client/chat_window.h"

#include <imgui.h>

#include <cfloat>
#include <deque>
#include <optional>
#include <string>

namespace nanommo {

namespace {

constexpr ImVec4 kName{0.45f, 0.75f, 1.0f, 1.0f};
constexpr ImVec4 kServer{1.0f, 0.8f, 0.4f, 1.0f};

}  // namespace

std::optional<std::string> ChatWindow::Build(
    const std::deque<protocol::Chat>& lines) {
  constexpr float kMargin = 10.0f;
  ImGui::SetNextWindowPos({kMargin, ImGui::GetIO().DisplaySize.y - kMargin},
                          ImGuiCond_Always, {0.0f, 1.0f});
  ImGui::SetNextWindowSize({420.0f, 200.0f});
  ImGui::SetNextWindowBgAlpha(0.4f);
  // Space presses the widget that ImGui's keyboard navigation is on, which
  // would open the field again after each line.
  ImGui::Begin("Chat", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoNavInputs |
                   ImGuiWindowFlags_NoFocusOnAppearing);

  ImGui::BeginChild("Lines", {0.0f, -ImGui::GetFrameHeightWithSpacing()});
  for (const protocol::Chat& line : lines) {
    // A line with no name is from the server.
    if (line.name().empty()) {
      ImGui::PushStyleColor(ImGuiCol_Text, kServer);
      ImGui::TextWrapped("%s", line.text().c_str());
      ImGui::PopStyleColor();
      continue;
    }
    ImGui::TextColored(kName, "%s:", line.name().c_str());
    ImGui::SameLine();
    ImGui::TextWrapped("%s", line.text().c_str());
  }
  // Follow the newest line, unless the player has scrolled up to read.
  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
    ImGui::SetScrollHereY(1.0f);
  }
  ImGui::EndChild();

  if (!ImGui::GetIO().WantTextInput &&
      ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
    ImGui::SetKeyboardFocusHere();
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
  ImGui::End();
  return said;
}

}  // namespace nanommo
