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
  switch (step_) {
    case Step::kAccount:
      return BuildAccount();
    case Step::kWorlds:
      return BuildWorlds();
    case Step::kCharacters:
      return BuildCharacters();
  }
  return std::nullopt;
}

void LoginScreen::Show(const protocol::LoginReply& reply) {
  error_ = reply.ok() ? "" : reply.error();
  // Without a session, the login failed, or the one we had ran out: back to
  // the login, with what the server said.
  if (reply.session().empty()) {
    session_.clear();
    step_ = Step::kAccount;
    return;
  }
  if (session_.empty()) {
    step_ = Step::kWorlds;
  }
  session_ = reply.session();
  if (reply.ok()) {
    worlds_ = {reply.worlds().begin(), reply.worlds().end()};
    selected_world_ = std::clamp(
        selected_world_, 0, std::max(static_cast<int>(worlds_.size()) - 1, 0));
  }
  characters_ = {reply.characters().begin(), reply.characters().end()};
  // The one just made, by its name, or else the one picked, while it's
  // still there, or else the first.
  const std::vector<const protocol::CharacterListing*> here = Here();
  const auto named = std::ranges::find_if(
      here, [&](const auto* c) { return c->name() == new_name_.data(); });
  if (reply.ok() && named != here.end() && new_name_[0] != '\0') {
    picked_ = (*named)->id();
    new_name_.fill('\0');
  } else if (std::ranges::none_of(
                 here, [&](const auto* c) { return c->id() == picked_; })) {
    picked_ = here.empty() ? 0 : here.front()->id();
  }
}

std::string LoginScreen::character_name() const {
  const auto found = std::ranges::find_if(
      characters_, [&](const auto& c) { return c.id() == picked_; });
  return found != characters_.end() ? found->name() : "";
}

protocol::LoginRequest LoginScreen::WithSession() const {
  protocol::LoginRequest request;
  request.set_session(session_);
  return request;
}

std::vector<const protocol::CharacterListing*> LoginScreen::Here() const {
  std::vector<const protocol::CharacterListing*> here;
  for (const protocol::CharacterListing& character : characters_) {
    if (world_ && character.world() == world_->name()) {
      here.push_back(&character);
    }
  }
  return here;
}

int LoginScreen::CountIn(const std::string& world) const {
  return static_cast<int>(std::ranges::count_if(
      characters_, [&](const auto& c) { return c.world() == world; }));
}

std::optional<protocol::LoginRequest> LoginScreen::BuildAccount() {
  ImGui::Begin("nanoMMO", nullptr, kWindowFlags);
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
  ShowErrorText(error_);
  ImGui::End();

  if (!log_in && !create) {
    return std::nullopt;
  }
  protocol::LoginRequest request;
  request.set_username(username_.data());
  request.set_password(password_.data());
  request.set_new_account(create);
  // We don't keep the password once it's on its way: the session the reply
  // brings stands in for it.
  password_.fill('\0');
  error_.clear();
  return request;
}

std::optional<protocol::LoginRequest> LoginScreen::BuildWorlds() {
  // The arrow keys and Enter pick a world, so ImGui's own keyboard navigation
  // stays out of this window.
  ImGui::Begin("Worlds", nullptr, kWindowFlags | ImGuiWindowFlags_NoNav);
  bool refresh =
      ImGui::IsWindowAppearing() || ImGui::IsKeyPressed(ImGuiKey_F5);
  const int count = static_cast<int>(worlds_.size());
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && selected_world_ + 1 < count) {
    ++selected_world_;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && selected_world_ > 0) {
    --selected_world_;
  }
  bool pick = ImGui::IsKeyPressed(ImGuiKey_Enter);

  for (int i = 0; i < count; ++i) {
    const protocol::WorldListing& world = worlds_[i];
    ImGui::PushID(i);
    if (ImGui::Selectable(world.name().c_str(), i == selected_world_, 0,
                          {160.0f, 0.0f})) {
      selected_world_ = i;
    }
    ImGui::SameLine();
    ImGui::TextDisabled(ICON_FA_USERS " %u", world.players());
    if (const int yours = CountIn(world.name()); yours > 0) {
      ImGui::SameLine();
      ImGui::TextDisabled(ICON_FA_USER " %d yours", yours);
    }
    if (world.content() != content_) {
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
  ImGui::SameLine();
  const bool log_out = ImGui::Button(ICON_FA_RIGHT_FROM_BRACKET " Log out");
  ShowErrorText(error_);
  ImGui::End();

  if (log_out) {
    session_.clear();
    characters_.clear();
    error_.clear();
    step_ = Step::kAccount;
    return std::nullopt;
  }
  if (pick && count > 0) {
    const protocol::WorldListing& picked = worlds_[selected_world_];
    // A client draws what the world sends from its own content, so with
    // other content, it would draw the wrong things, or nothing.
    if (picked.content() != content_) {
      error_ = absl::StrCat(picked.name(),
                            " has other content than this client, so they "
                            "can't play together.");
    } else {
      world_ = picked;
      error_.clear();
      deleting_ = false;
      step_ = Step::kCharacters;
      const std::vector<const protocol::CharacterListing*> here = Here();
      picked_ = here.empty() ? 0 : here.front()->id();
      return std::nullopt;
    }
  }
  if (!refresh) {
    return std::nullopt;
  }
  return WithSession();
}

std::optional<protocol::LoginRequest> LoginScreen::BuildCharacters() {
  const std::vector<const protocol::CharacterListing*> here = Here();
  const auto picked = std::ranges::find_if(
      here, [&](const auto* c) { return c->id() == picked_; });
  const protocol::CharacterListing* character =
      picked != here.end() ? *picked : nullptr;

  ImGui::Begin(world_->name().c_str(), nullptr,
               kWindowFlags | ImGuiWindowFlags_NoNav);
  // Back from the world, or in from the worlds: the list as it is now.
  const bool refresh = ImGui::IsWindowAppearing();
  const bool typing = ImGui::GetIO().WantTextInput;
  if (!typing && !here.empty() && picked != here.end()) {
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && picked + 1 != here.end()) {
      picked_ = (*(picked + 1))->id();
      deleting_ = false;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && picked != here.begin()) {
      picked_ = (*(picked - 1))->id();
      deleting_ = false;
    }
  }

  // The characters, and a new one, on the left.
  ImGui::BeginGroup();
  ImGui::TextDisabled("Your characters");
  for (const protocol::CharacterListing* each : here) {
    ImGui::PushID(static_cast<int>(each->id()));
    if (ImGui::Selectable(each->name().c_str(), each->id() == picked_, 0,
                          {200.0f, 0.0f})) {
      picked_ = each->id();
      deleting_ = false;
    }
    ImGui::TextDisabled("  Level %u, %s", each->level(), each->land().c_str());
    ImGui::PopID();
  }
  if (here.empty()) {
    ImGui::TextDisabled("Nobody yet. Make a character:");
  }
  ImGui::Separator();
  ImGui::SetNextItemWidth(200.0f);
  const bool named = ImGui::InputTextWithHint(
      "##name", "A name", new_name_.data(), new_name_.size(),
      ImGuiInputTextFlags_EnterReturnsTrue);
  const bool create =
      ImGui::Button(ICON_FA_USER_PLUS " New character") || named;
  ImGui::EndGroup();

  // The one picked, on the right.
  ImGui::SameLine();
  ImGui::BeginGroup();
  bool enter = false;
  bool delete_it = false;
  if (character != nullptr) {
    ImGui::Text("%s", character->name().c_str());
    ImGui::TextDisabled("Level %u, in the %s", character->level(),
                        character->land().c_str());
    if (!deleting_) {
      enter = ImGui::Button(ICON_FA_PLAY " Enter the world") ||
              (!typing && ImGui::IsKeyPressed(ImGuiKey_Enter));
      ImGui::SameLine();
      deleting_ = ImGui::Button(ICON_FA_TRASH " Delete");
    } else {
      ImGui::TextColored({1.0f, 0.7f, 0.4f, 1.0f},
                         "Delete %s for good?", character->name().c_str());
      delete_it = ImGui::Button(ICON_FA_TRASH " Delete");
      ImGui::SameLine();
      if (ImGui::Button("Keep")) {
        deleting_ = false;
      }
    }
  }
  ImGui::EndGroup();

  const bool back = ImGui::Button(ICON_FA_ARROW_LEFT " Worlds");
  ShowErrorText(error_);
  ImGui::End();

  if (back) {
    step_ = Step::kWorlds;
    error_.clear();
    deleting_ = false;
    return std::nullopt;
  }
  protocol::LoginRequest request = WithSession();
  if (enter && character != nullptr) {
    request.set_enter(true);
    request.set_character(character->id());
  } else if (delete_it && character != nullptr) {
    request.set_delete_character(true);
    request.set_character(character->id());
    deleting_ = false;
  } else if (create) {
    request.set_create_character(true);
    request.set_world(world_->name());
    request.set_name(new_name_.data());
  } else if (!refresh) {
    return std::nullopt;
  }
  error_.clear();
  return request;
}

}  // namespace nanommo
