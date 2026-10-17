#include "client/login_screen.h"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "extras/IconsFontAwesome6.h"

namespace nanommo {

namespace {

constexpr ImGuiWindowFlags kWindowFlags = ImGuiWindowFlags_AlwaysAutoResize |
                                          ImGuiWindowFlags_NoCollapse |
                                          ImGuiWindowFlags_NoMove;

void ShowErrorText(const std::string& error) {
  if (!error.empty()) {
    ImGui::TextColored({1.0f, 0.45f, 0.4f, 1.0f},
                       ICON_FA_TRIANGLE_EXCLAMATION " %s", error.c_str());
  }
}

}  // namespace

std::optional<protocol::LoginRequest> LoginScreen::Build() {
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Always, {0.5f, 0.5f});
  return world_ ? BuildLogin(absl::StrCat("Log in to ", world_->name()))
                : BuildWorlds();
}

void LoginScreen::ShowWorlds(std::vector<protocol::WorldListing> worlds) {
  worlds_ = std::move(worlds);
  selected_ = std::clamp(selected_, 0,
                         std::max(static_cast<int>(worlds_.size()) - 1, 0));
}

std::optional<protocol::LoginRequest> LoginScreen::BuildWorlds() {
  // The arrow keys and Enter pick a world, so ImGui's own keyboard navigation
  // stays out of this window.
  ImGui::Begin("Worlds", nullptr, kWindowFlags | ImGuiWindowFlags_NoNav);
  bool refresh = ImGui::IsWindowAppearing() || ImGui::IsKeyPressed(ImGuiKey_F5);
  const int count = static_cast<int>(worlds_.size());
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && selected_ + 1 < count) {
    ++selected_;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && selected_ > 0) {
    --selected_;
  }
  bool pick = ImGui::IsKeyPressed(ImGuiKey_Enter);

  for (int i = 0; i < count; ++i) {
    ImGui::PushID(i);
    if (ImGui::Selectable(worlds_[i].name().c_str(), i == selected_, 0,
                          {160.0f, 0.0f})) {
      selected_ = i;
    }
    ImGui::SameLine();
    ImGui::TextDisabled(ICON_FA_USERS " %u", worlds_[i].players());
    if (worlds_[i].content() != content_) {
      ImGui::SameLine();
      ImGui::TextDisabled(ICON_FA_TRIANGLE_EXCLAMATION " other content");
    }
    ImGui::PopID();
  }
  if (count == 0) {
    ImGui::TextDisabled("No worlds are up.");
  }
  refresh = ImGui::Button(ICON_FA_ARROWS_ROTATE " Refresh") || refresh;
  ImGui::SameLine();
  pick = ImGui::Button(ICON_FA_ARROW_RIGHT " Continue") || pick;
  ShowErrorText(error_);
  ImGui::End();

  if (pick && count > 0) {
    const protocol::WorldListing& picked = worlds_[selected_];
    // A client draws what the world sends from its own content, so with
    // other content, it would draw the wrong things, or nothing.
    if (picked.content() != content_) {
      error_ = absl::StrCat(picked.name(),
                            " has other content than this client, so they "
                            "can't play together.");
    } else {
      world_ = picked;
      error_.clear();
    }
  }
  if (!refresh) {
    return std::nullopt;
  }
  protocol::LoginRequest request;
  request.set_list_worlds(true);
  return request;
}

std::optional<protocol::LoginRequest> LoginScreen::BuildLogin(
    const std::string& title) {
  ImGui::Begin(title.c_str(), nullptr, kWindowFlags);

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
  ImGui::SameLine();
  const bool back = ImGui::Button(ICON_FA_ARROW_LEFT " Worlds");
  ShowErrorText(error_);
  ImGui::End();

  if (back) {
    world_.reset();
    error_.clear();
    return std::nullopt;
  }
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
