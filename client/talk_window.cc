#include "client/talk_window.h"

#include <imgui.h>

#include <cfloat>
#include <optional>
#include <string>
#include <utility>

#include "client/theme.h"

namespace nanommo {

void TalkWindow::Open(uint32_t npc, std::string name) {
  npc_ = npc;
  name_ = std::move(name);
  focus_ = true;
}

std::optional<protocol::Talk> TalkWindow::Build() {
  if (!npc_) {
    return std::nullopt;
  }
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x / 2, display.y * 0.75f},
                          ImGuiCond_Appearing, {0.5f, 0.5f});
  ImGui::SetNextWindowSize({Ui(380.0f), 0.0f});
  bool open = true;
  BeginPanel(name_.c_str(), &open,
             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
  ImGui::TextWrapped("Give me a few words, and I'll tell you a story.");
  // So that the player can type straight away.
  if (focus_) {
    ImGui::SetKeyboardFocusHere();
    focus_ = false;
  }
  ImGui::SetNextItemWidth(-FLT_MIN);
  const bool entered = ImGui::InputTextWithHint(
      "##words", "dragon, castle, gold", words_.data(), words_.size(),
      ImGuiInputTextFlags_EnterReturnsTrue);
  const bool asked = GameButton("Tell me a story", {-1.0f, 0.0f}, true);
  std::optional<protocol::Talk> talk;
  if ((entered || asked) && words_[0] != '\0') {
    talk.emplace();
    talk->set_npc(*npc_);
    talk->set_words(words_.data());
    words_.fill('\0');
    open = false;
  }
  EndPanel();
  if (!open) {
    npc_.reset();
  }
  return talk;
}

}  // namespace nanommo
