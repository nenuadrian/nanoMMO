#include "client/login_screen.h"

#include <imgui.h>
#include <raylib.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "client/theme.h"
#include "extras/IconsFontAwesome6.h"
#include "protocol/version.h"

namespace nanommo {

namespace {

constexpr ImGuiWindowFlags kWindowFlags = ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoResize |
                                          ImGuiWindowFlags_NoSavedSettings;

// A window that's only a place to draw in, and to take the mouse.
constexpr ImGuiWindowFlags kBare =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;

// How far a pixel of a drag turns the character, in radians, and how near and
// far the preview's camera goes, in metres.
constexpr float kTurnPerPixel = 0.012f;
constexpr float kNearest = 2.2f;
constexpr float kFarthest = 5.5f;
constexpr float kZoomStep = 0.3f;

// In points, from the corner of the window to the version.
constexpr float kCornerMargin = 14.0f;

void ShowErrorText(const std::string& error) {
  if (!error.empty()) {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored({1.0f, 0.45f, 0.4f, 1.0f},
                       ICON_FA_TRIANGLE_EXCLAMATION " %s", error.c_str());
    ImGui::PopTextWrapPos();
  }
}

// A row to pick, as a world or a character is: its name, in carved capitals,
// with a line under it, lit while it's picked, with an ember's edge. Returns
// whether it was clicked, and sets twice when that was a double click.
bool Row(const char* id, const std::string& name, const std::string& line,
         bool picked, bool* twice = nullptr) {
  const float height = Ui(58.0f);
  const float width = ImGui::GetContentRegionAvail().x;
  const bool clicked = ImGui::InvisibleButton(id, {width, height});
  const bool hovered = ImGui::IsItemHovered();
  if (twice != nullptr) {
    *twice = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
  }
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  if (picked) {
    draw->AddRectFilledMultiColor(
        min, max, Faded(colors::kCrimson, 0.55f), Faded(colors::kWine, 0.2f),
        Faded(colors::kWine, 0.2f), Faded(colors::kCrimson, 0.55f));
    draw->AddRectFilled(min, {min.x + Ui(3.0f), max.y}, colors::kEmber);
    draw->AddRect(min, max, Faded(colors::kGoldDark, 0.7f), 0.0f, 0, Ui(1.0f));
  } else if (hovered) {
    draw->AddRectFilled(min, max, Faded(colors::kWine, 0.45f));
  }
  DrawGradientText(draw, GetFonts().title, Ui(18.0f),
                   {min.x + Ui(14.0f), min.y + Ui(8.0f)},
                   picked ? colors::kGoldLight : colors::kPaper,
                   picked ? colors::kGold : colors::kMuted, name.c_str());
  DrawShadowedText(draw, GetFonts().text, Ui(15.0f),
                   {min.x + Ui(14.0f), min.y + Ui(32.0f)}, colors::kMuted,
                   line.c_str());
  return clicked;
}

}  // namespace

Figure LoginScreen::FigureOf(const protocol::CharacterListing& character,
                             const ItemTable& items) {
  Figure figure;
  figure.id = character.id();
  figure.name = character.name();
  figure.level = static_cast<int>(character.level());
  figure.weapon = character.weapon();
  figure.wearing = {character.wearing().begin(), character.wearing().end()};
  // A character that's never entered gets the kit as it does: it's shown in
  // it now, the way the world will dress it.
  if (character.fresh()) {
    std::vector<items::Slot> taken;
    for (const items::Stack& stack : items.starting) {
      const auto item = items.by_id.find(stack.item());
      if (item == items.by_id.end() || !item->second.has_equip()) {
        continue;
      }
      const items::Slot slot = item->second.equip().slot();
      if (std::ranges::find(taken, slot) != taken.end()) {
        continue;
      }
      taken.push_back(slot);
      if (slot == items::SLOT_MAIN_HAND) {
        figure.weapon = stack.item();
      } else {
        figure.wearing.push_back(stack.item());
      }
    }
  }
  figure.from = protocol::MOTION_IDLE;
  figure.to = protocol::MOTION_IDLE;
  figure.blend = 1.0f;
  return figure;
}

std::optional<protocol::LoginRequest> LoginScreen::Build() {
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  ImDrawList* back = ImGui::GetBackgroundDrawList();
  DrawBackdrop(back, screen, static_cast<float>(ImGui::GetTime()));
  // The game's name, high up, and smaller over the characters, which need the
  // room.
  const bool characters = step_ == Step::kCharacters;
  DrawLogo(back, {screen.x / 2.0f, screen.y * (characters ? 0.035f : 0.14f)},
           Ui(characters ? 44.0f : 92.0f));

  // The version, in the corner, as most other games show it, so a player can
  // tell which one they have.
  const std::string version = absl::StrCat("nanoMMO ", kVersion);
  ImFont* font = GetFonts().text;
  const float size = Ui(14.0f);
  const ImVec2 extent =
      font->CalcTextSizeA(size, FLT_MAX, 0.0f, version.c_str());
  back->AddText(font, size,
                {screen.x - extent.x - Ui(kCornerMargin),
                 screen.y - extent.y - Ui(kCornerMargin)},
                colors::kFaint, version.c_str());

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
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({screen.x / 2.0f, screen.y * 0.55f}, ImGuiCond_Always,
                          {0.5f, 0.5f});
  ImGui::SetNextWindowSize({Ui(380.0f), 0.0f});
  BeginPanel("Your account", nullptr, kWindowFlags);
  ImGui::TextDisabled("Name");
  if (ImGui::IsWindowAppearing()) {
    ImGui::SetKeyboardFocusHere();
  }
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::InputText("##name", username_.data(), username_.size());
  ImGui::TextDisabled("Password");
  ImGui::SetNextItemWidth(-FLT_MIN);
  const bool enter = ImGui::InputText(
      "##password", password_.data(), password_.size(),
      ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::Spacing();
  const bool log_in = GameButton("Log in", {-1.0f, 0.0f}, true) || enter;
  const bool create = GameButton("Make an account", {-1.0f, 0.0f});
  ShowErrorText(error_);
  EndPanel();

  if (!log_in && !create) {
    return std::nullopt;
  }
  protocol::LoginRequest request;
  request.set_username(username_.data());
  request.set_password(password_.data());
  request.set_new_account(create);
  request.set_version(kVersion);
  // We don't keep the password once it's on its way: the session the reply
  // brings stands in for it.
  password_.fill('\0');
  error_.clear();
  return request;
}

std::optional<protocol::LoginRequest> LoginScreen::BuildWorlds() {
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({screen.x / 2.0f, screen.y * 0.55f}, ImGuiCond_Always,
                          {0.5f, 0.5f});
  ImGui::SetNextWindowSize({Ui(480.0f), 0.0f});
  // The arrow keys and Enter pick a world, so ImGui's own keyboard navigation
  // stays out of this window.
  BeginPanel("Choose a world", nullptr, kWindowFlags | ImGuiWindowFlags_NoNav);
  bool refresh = ImGui::IsWindowAppearing() || ImGui::IsKeyPressed(ImGuiKey_F5);
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
    std::string line = absl::StrCat(world.players(), " playing");
    if (const int yours = CountIn(world.name()); yours > 0) {
      absl::StrAppend(&line, ", ", yours, " of your characters");
    }
    if (world.content() != content_) {
      absl::StrAppend(&line, ", other content");
    }
    ImGui::PushID(i);
    bool twice = false;
    if (Row("##world", world.name(), line, i == selected_world_, &twice)) {
      selected_world_ = i;
    }
    pick = pick || twice;
    ImGui::PopID();
  }
  if (count == 0) {
    ImGui::TextDisabled("No worlds are up.");
  }
  ImGui::Spacing();
  const float third = (ImGui::GetContentRegionAvail().x -
                       2.0f * ImGui::GetStyle().ItemSpacing.x) /
                      3.0f;
  const bool log_out = GameButton("Log out", {third, 0.0f});
  ImGui::SameLine();
  refresh = GameButton("Refresh", {third, 0.0f}) || refresh;
  ImGui::SameLine();
  pick = GameButton("Continue", {third, 0.0f}, true) || pick;
  ShowErrorText(error_);
  EndPanel();

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
  if (!world_) {
    return std::nullopt;
  }
  const protocol::WorldListing& world = *world_;
  const std::vector<const protocol::CharacterListing*> here = Here();
  const auto picked = std::ranges::find_if(
      here, [&](const auto* c) { return c->id() == picked_; });
  const protocol::CharacterListing* character =
      picked != here.end() ? *picked : nullptr;
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
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

  // Back to the worlds, in the top left.
  ImGui::SetNextWindowPos({Ui(24.0f), Ui(24.0f)});
  ImGui::Begin("##back", nullptr, kBare | ImGuiWindowFlags_AlwaysAutoResize);
  const bool back = GameButton("«  Worlds", {0.0f, 0.0f});
  ImGui::End();

  // The one picked, on its stage, in the middle, to turn and look at.
  const ImVec2 stage{Ui(380.0f), Ui(506.0f)};
  const ImVec2 stage_min{screen.x * 0.42f - stage.x / 2.0f,
                         screen.y * 0.5f - stage.y / 2.0f};
  ImGui::SetNextWindowPos(stage_min);
  ImGui::SetNextWindowSize({stage.x, stage.y + Ui(90.0f)});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
  ImGui::Begin("##stage", nullptr, kBare);
  ImGui::PopStyleVar();
  if (character != nullptr) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 stage_max{stage_min.x + stage.x, stage_min.y + stage.y};
    // A pool of light on the floor, and the fire's glow behind.
    const ImVec2 floor{(stage_min.x + stage_max.x) / 2.0f,
                       stage_max.y - stage.y * 0.035f};
    for (int ring = 8; ring >= 1; --ring) {
      const float share = static_cast<float>(ring) / 8.0f;
      draw->AddEllipseFilled({floor.x, floor.y - stage.y * 0.4f},
                             {stage.x * 0.55f * share, stage.y * 0.55f * share},
                             Faded(colors::kCrimson, 0.035f));
    }
    for (int ring = 5; ring >= 1; --ring) {
      const float share = static_cast<float>(ring) / 5.0f;
      draw->AddEllipseFilled(floor,
                             {stage.x * 0.4f * share, stage.x * 0.1f * share},
                             Faded(colors::kFlame, 0.07f));
    }
    draw->AddEllipse(floor, {stage.x * 0.34f, stage.x * 0.085f},
                     Faded(colors::kGold, 0.7f), 0.0f, 0, Ui(1.5f));
    ImGui::SetCursorScreenPos(stage_min);
    ImGui::InvisibleButton("##preview", stage);
    // OpenGL keeps the rows bottom up, so the UVs turn the picture over.
    draw->AddImage(static_cast<ImTextureID>(preview_.texture().id), stage_min,
                   stage_max, {0.0f, 1.0f}, {1.0f, 0.0f});
    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsItemActive()) {
      turn_ += io.MouseDelta.x * kTurnPerPixel / UiScale();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
      distance_ = std::clamp(distance_ - io.MouseWheel * kZoomStep, kNearest,
                             kFarthest);
    }
    // Its name, under it.
    ImFont* title = GetFonts().title;
    const float name_size = Ui(30.0f);
    const ImVec2 name_extent = title->CalcTextSizeA(name_size, FLT_MAX, 0.0f,
                                                    character->name().c_str());
    DrawGradientText(draw, title, name_size,
                     {floor.x - name_extent.x / 2.0f, stage_max.y + Ui(6.0f)},
                     colors::kGoldLight, colors::kGold,
                     character->name().c_str());
    const std::string line = absl::StrCat("Level ", character->level(),
                                          ", in the ", character->land());
    const float line_size = Ui(17.0f);
    const ImVec2 line_extent =
        GetFonts().text->CalcTextSizeA(line_size, FLT_MAX, 0.0f, line.c_str());
    DrawShadowedText(draw, GetFonts().text, line_size,
                     {floor.x - line_extent.x / 2.0f,
                      stage_max.y + Ui(10.0f) + name_extent.y},
                     colors::kMuted, line.c_str());
  }
  ImGui::End();

  // The characters, and a new one, on the right.
  ImGui::SetNextWindowPos({screen.x - Ui(48.0f), screen.y * 0.48f},
                          ImGuiCond_Always, {1.0f, 0.5f});
  ImGui::SetNextWindowSize({Ui(360.0f), 0.0f});
  BeginPanel((world.name() + "###characters").c_str(), nullptr,
             kWindowFlags | ImGuiWindowFlags_NoNav);
  // Back from the world, or in from the worlds: the list as it is now.
  const bool refresh = ImGui::IsWindowAppearing();
  for (const protocol::CharacterListing* each : here) {
    ImGui::PushID(static_cast<int>(each->id()));
    if (Row("##character", each->name(),
            absl::StrCat("Level ", each->level(), ", ", each->land()),
            each->id() == picked_)) {
      picked_ = each->id();
      deleting_ = false;
    }
    ImGui::PopID();
  }
  if (here.empty()) {
    ImGui::TextDisabled("Nobody yet. Make a character:");
  }
  ImGui::Spacing();
  ImGui::SeparatorText("A new character");
  ImGui::SetNextItemWidth(-FLT_MIN);
  const bool named = ImGui::InputTextWithHint(
      "##name", "Their name", new_name_.data(), new_name_.size(),
      ImGuiInputTextFlags_EnterReturnsTrue);
  const bool create = GameButton("Make them", {-1.0f, 0.0f}) || named;
  ShowErrorText(error_);
  EndPanel();

  // Into the world, at the bottom, with a way to delete the one picked.
  bool enter = false;
  bool delete_it = false;
  if (character != nullptr) {
    ImGui::SetNextWindowPos({screen.x * 0.42f, screen.y - Ui(36.0f)},
                            ImGuiCond_Always, {0.5f, 1.0f});
    ImGui::Begin("##enter", nullptr, kBare | ImGuiWindowFlags_AlwaysAutoResize);
    if (!deleting_) {
      enter = GameButton("Enter the world", Ui(ImVec2{300.0f, 52.0f}), true) ||
              (!typing && ImGui::IsKeyPressed(ImGuiKey_Enter));
      ImGui::SameLine();
      ImGui::SetCursorPosY(ImGui::GetCursorPosY() + Ui(9.0f));
      deleting_ = GameButton("Delete", {0.0f, 0.0f});
    } else {
      ImGui::TextColored({1.0f, 0.7f, 0.4f, 1.0f}, "Delete %s for good?",
                         character->name().c_str());
      delete_it = GameButton("Delete", {Ui(140.0f), 0.0f}, true);
      ImGui::SameLine();
      if (GameButton("Keep", {Ui(140.0f), 0.0f})) {
        deleting_ = false;
      }
    }
    ImGui::End();
  }

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
    request.set_world(world.name());
    request.set_name(new_name_.data());
  } else if (!refresh) {
    return std::nullopt;
  }
  error_.clear();
  return request;
}

void LoginScreen::Render(const Renderer& renderer) {
  if (step_ != Step::kCharacters || !world_) {
    return;
  }
  const auto found = std::ranges::find_if(
      characters_, [&](const auto& c) { return c.id() == picked_; });
  if (found == characters_.end()) {
    return;
  }
  Figure figure = FigureOf(*found, items_);
  figure.yaw = turn_;
  // Its idle, from when the client started: it breathes, and looks round.
  figure.to_time = static_cast<float>(GetTime());
  // The camera looks at the middle, and closer up, higher, so that the top of
  // the picture stays two metres up and the head stays in it.
  constexpr float kFov = 35.0f;
  constexpr float kTop = 2.0f;
  const float height =
      std::max(0.9f, kTop - distance_ * std::tan(kFov / 2.0f * DEG2RAD));
  const Camera3D camera{
      .position = {0.0f, height + 0.1f, distance_},
      .target = {0.0f, height, 0.0f},
      .up = {0.0f, 1.0f, 0.0f},
      .fovy = kFov,
      .projection = CAMERA_PERSPECTIVE,
  };
  preview_.RenderPreview(renderer, figure, camera);
}

}  // namespace nanommo
