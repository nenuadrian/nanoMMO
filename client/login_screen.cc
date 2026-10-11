#include "client/login_screen.h"

#include <imgui.h>

#include "extras/IconsFontAwesome6.h"

namespace nanommo {

std::optional<protocol::LoginRequest> LoginScreen::Build() {
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Always, {0.5f, 0.5f});
  ImGui::Begin("Log in", nullptr,
               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoMove);

  if (ImGui::IsWindowAppearing()) {
    ImGui::SetKeyboardFocusHere();
  }
  ImGui::InputText("Name", username_.data(), username_.size());
  const bool enter = ImGui::InputText(
      "Password", password_.data(), password_.size(),
      ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
  const bool log_in =
      ImGui::Button(ICON_FA_RIGHT_TO_BRACKET " Log in") || enter;
  ImGui::SameLine();
  const bool create = ImGui::Button(ICON_FA_USER_PLUS " Create account");
  if (!error_.empty()) {
    ImGui::TextColored({1.0f, 0.45f, 0.4f, 1.0f},
                       ICON_FA_TRIANGLE_EXCLAMATION " %s", error_.c_str());
  }
  ImGui::End();

  if (!log_in && !create) {
    return std::nullopt;
  }
  protocol::LoginRequest request;
  request.set_username(username_.data());
  request.set_password(password_.data());
  request.set_new_account(create);
  // We don't keep the password once it's on its way.
  password_.fill('\0');
  error_.clear();
  return request;
}

}  // namespace nanommo
