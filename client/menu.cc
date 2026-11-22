#include "client/menu.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "client/theme.h"
#include "client/translations.h"
#include "extras/IconsFontAwesome6.h"

namespace nanommo {

namespace {

// A slider from 0 to 100 percent, for a setting from nothing to full.
void SliderPercent(const char* label, float& value) {
  int percent = static_cast<int>(std::lround(value * 100.0f));
  // Ctrl and a click lets a player type a number, which ImGui takes past the
  // slider's ends unless it's told to clamp it, and 500% would be far too loud.
  if (ImGui::SliderInt(label, &percent, 0, 100, "%d%%",
                       ImGuiSliderFlags_AlwaysClamp)) {
    value = static_cast<float>(percent) / 100.0f;
  }
}

// The keys, and what each does, for the keys' page and the help's.
constexpr std::array<std::pair<const char*, const char*>, 21> kKeys = {{
    {"W A S D, or the arrows", "Walk"},
    {"Shift", "Run"},
    {"Space", "Swing what you hold"},
    {"1 to 9, and with Shift", "The hotbar's two rows of skills"},
    {"Click a monster", "Pick it, to fight"},
    {"Double-click the ground", "Walk there"},
    {"Click someone", "Talk, learn or take a quest"},
    {"Q and E, or hold the right button", "Turn the camera"},
    {"Mouse wheel", "Zoom in and out"},
    {"M", "The map"},
    {"C", "Your character"},
    {"I", "Your bag"},
    {"T", "Your quests"},
    {"G", "Your guild"},
    {"K", "Crafting"},
    {"H", "Help"},
    {"O", "Settings"},
    {"Enter", "Chat"},
    {"Enter, then /g and a space", "Talk to your guild"},
    {"Escape", "Close what's open, or this menu"},
    {"Drag an item out of the bag", "Throw it away"},
}};

// In points: a round button's radius, as the chat's is, and the room round
// the row for the buttons' rims, which the window would cut.
constexpr float kRound = 23.0f;
constexpr float kRimRoom = 4.0f;

// A button in the bottom right row, with an icon, round, as the chat's is.
bool BarButton(const char* id, const char* icon, bool on, const char* tip) {
  const float radius = Ui(kRound);
  const bool clicked =
      ImGui::InvisibleButton(id, {radius * 2.0f, radius * 2.0f});
  const bool hovered = ImGui::IsItemHovered();
  const ImVec2 min = ImGui::GetItemRectMin();
  DrawRoundButton(ImGui::GetWindowDrawList(), {min.x + radius, min.y + radius},
                  radius, icon, hovered, on);
  if (hovered) {
    ImGui::SetTooltip("%s", tip);
  }
  return clicked;
}

}  // namespace

void BuildKeys() {
  if (ImGui::BeginTable("Keys", 2, ImGuiTableFlags_RowBg)) {
    for (const auto& [key, does] : kKeys) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::PushFont(GetFonts().bold, 0.0f);
      ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(colors::kGold), "%s",
                         key);
      ImGui::PopFont();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(does);
    }
    ImGui::EndTable();
  }
}

Menu::Choice Menu::Build(Settings& settings, bool* reset_hotbar) {
  Choice choice = Choice::kNothing;
  if (open_) {
    // The world, dimmed behind it.
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        {0.0f, 0.0f}, ImGui::GetIO().DisplaySize, Faded(IM_COL32_BLACK, 0.45f));
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({display.x / 2.0f, display.y * 0.45f},
                            ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({Ui(300.0f), 0.0f});
    BeginPanel("Menu", &open_,
               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoSavedSettings);
    const ImVec2 button{-1.0f, Ui(40.0f)};
    if (GameButton("Back to the game", button, true)) {
      open_ = false;
    }
    if (GameButton("Settings", button)) {
      settings_ = true;
      open_ = false;
    }
    if (GameButton("Log out", button)) {
      choice = Choice::kLogOut;
      open_ = false;
    }
    if (GameButton("Quit", button)) {
      choice = Choice::kQuit;
    }
    ImGui::Spacing();
    EndPanel();
  }
  if (settings_) {
    BuildSettings(settings, reset_hotbar);
  }
  return choice;
}

void Menu::BuildSettings(Settings& settings, bool* reset_hotbar) {
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x / 2.0f, display.y / 2.0f},
                          ImGuiCond_Appearing, {0.5f, 0.5f});
  ImGui::SetNextWindowSize(Ui(ImVec2{700.0f, 470.0f}));
  BeginPanel("Settings", &settings_,
             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

  // The pages down the left, each with its English name as its ID, and the
  // one picked on the right.
  struct Tab {
    Page page;
    const char* icon;
    const char* name;
  };
  constexpr std::array<Tab, 4> kPages = {{
      {Page::kInterface, ICON_FA_TABLE_COLUMNS, N_("Interface")},
      {Page::kSound, ICON_FA_VOLUME_HIGH, N_("Sound")},
      {Page::kGraphics, ICON_FA_DISPLAY, N_("Graphics")},
      {Page::kKeys, ICON_FA_KEYBOARD, N_("Keys")},
  }};
  ImGui::BeginChild("Pages", {Ui(170.0f), 0.0f});
  for (const auto& [page, icon, name] : kPages) {
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, {0.0f, 0.5f});
    const std::string label = absl::StrCat(icon, "  ", Tr(name), "###", name);
    if (ImGui::Selectable(label.c_str(), page_ == page, 0, {0.0f, Ui(34.0f)})) {
      page_ = page;
    }
    ImGui::PopStyleVar();
  }
  ImGui::EndChild();
  ImGui::SameLine(0.0f, Ui(16.0f));
  ImGui::BeginChild("Page");
  ImGui::PushItemWidth(Ui(260.0f));
  switch (page_) {
    case Page::kInterface: {
      ImGui::SeparatorText(Tr("Interface"));
      // Each language by its own name, and it changes as it's picked.
      const auto chosen =
          std::ranges::find(kLanguages, settings.ui.language, &Language::code);
      if (ImGui::BeginCombo(TrLabel("Language"),
                            chosen != kLanguages.end()
                                ? chosen->name
                                : settings.ui.language.c_str())) {
        for (const Language& language : kLanguages) {
          if (ImGui::Selectable(language.name,
                                settings.ui.language == language.code)) {
            settings.ui.language = language.code;
          }
        }
        ImGui::EndCombo();
      }
      if (!ImGui::IsAnyItemActive() || scale_ == 0.0f) {
        scale_ = settings.ui.scale;
      }
      int percent = static_cast<int>(std::lround(scale_ * 100.0f));
      if (ImGui::SliderInt(TrLabel("Size"), &percent, 75, 150, "%d%%",
                           ImGuiSliderFlags_AlwaysClamp)) {
        scale_ = static_cast<float>(percent) / 100.0f;
      }
      if (ImGui::IsItemDeactivatedAfterEdit()) {
        settings.ui.scale = scale_;
      }
      ImGui::TextDisabled(
          "%s", Tr("On top of its size for your screen, which grows with it."));
      ImGui::Spacing();
      ImGui::Checkbox(TrLabel("Show the minimap"), &settings.ui.minimap);
      ImGui::BeginDisabled(!settings.ui.minimap);
      ImGui::Checkbox(TrLabel("Turn the minimap with the camera"),
                      &settings.ui.minimap_turns);
      ImGui::EndDisabled();
      ImGui::Spacing();
      if (GameButton("Put the hotbar back in order", {0.0f, 0.0f})) {
        settings.ui.hotbar.clear();
        *reset_hotbar = true;
      }
      ImGui::TextDisabled("%s", Tr("Drag a skill onto another to swap them."));

      // Whose names show over their heads, and what with.
      ImGui::SeparatorText(Tr("Names over heads"));
      ImGui::TextDisabled(
          "%s", Tr("Each shows while you point at it, or it's picked."));
      ImGui::TextDisabled("%s", Tr("Always show them over:"));
      NameplateSettings& plates = settings.ui.nameplates;
      ImGui::Checkbox(TrLabel("Monsters"), &plates.monsters);
      ImGui::SameLine(Ui(150.0f));
      ImGui::Checkbox(TrLabel("Other players"), &plates.players);
      ImGui::Checkbox(TrLabel("NPCs"), &plates.npcs);
      ImGui::SameLine(Ui(150.0f));
      ImGui::Checkbox(TrLabel("You"), &plates.yours);
      ImGui::TextDisabled("%s", Tr("With them:"));
      ImGui::Checkbox(TrLabel("Health"), &plates.health);
      ImGui::SameLine(Ui(150.0f));
      ImGui::Checkbox(TrLabel("Levels"), &plates.levels);
      ImGui::Checkbox(TrLabel("Guilds"), &plates.guilds);
      break;
    }
    case Page::kSound:
      ImGui::SeparatorText(Tr("Sound"));
      SliderPercent(TrLabel("Volume"), settings.sound.master);
      SliderPercent(TrLabel("Music"), settings.sound.music);
      SliderPercent(TrLabel("Effects"), settings.sound.effects);
      ImGui::Checkbox(TrLabel("Mute"), &settings.sound.muted);
      ImGui::Spacing();
      ImGui::TextDisabled(
          ICON_FA_MUSIC " %s",
          Tr("\"Background Music\" by Pavel Bekirov, from Pixabay"));
      break;
    case Page::kGraphics:
      ImGui::SeparatorText(Tr("Graphics"));
      ImGui::Checkbox(TrLabel("Bloom"), &settings.graphics.bloom);
      ImGui::Checkbox(TrLabel("Cel shading"), &settings.graphics.cel_shading);
      ImGui::Checkbox(TrLabel("Anti-aliasing"), &settings.graphics.antialias);
      ImGui::SeparatorText(
          absl::StrCat(ICON_FA_CLOUD_SUN "  ", Tr("Sky")).c_str());
      ImGui::Checkbox(TrLabel("Weather"), &settings.graphics.weather);
      ImGui::Checkbox(TrLabel("Day and night"), &settings.graphics.day_night);
      break;
    case Page::kKeys:
      ImGui::SeparatorText(Tr("Keys"));
      BuildKeys();
      break;
  }
  ImGui::PopItemWidth();
  ImGui::EndChild();
  EndPanel();
}

void Menu::BuildButtons(bool* character, bool* bag, bool* quests, bool* guild,
                        bool* map, bool* help) {
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  // As far in from the corner as the chat's button is from its own, with room
  // round them for their rims, and nothing behind them.
  const float room = Ui(kRimRoom);
  ImGui::SetNextWindowPos(
      {display.x - Ui(18.0f) + room, display.y - Ui(18.0f) + room},
      ImGuiCond_Always, {1.0f, 1.0f});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {room, room});
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Ui(ImVec2{10.0f, 10.0f}));
  ImGui::Begin("Buttons", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav |
                   ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoFocusOnAppearing |
                   ImGuiWindowFlags_NoSavedSettings);
  if (BarButton("##character", ICON_FA_USER, *character, "Character (C)")) {
    *character = !*character;
  }
  ImGui::SameLine();
  if (BarButton("##bag", ICON_FA_BAG_SHOPPING, *bag, "Bag (I)")) {
    *bag = !*bag;
  }
  ImGui::SameLine();
  if (BarButton("##quests", ICON_FA_SCROLL, *quests, "Quests (T)")) {
    *quests = !*quests;
  }
  ImGui::SameLine();
  if (BarButton("##guild", ICON_FA_SHIELD_HALVED, *guild, "Guild (G)")) {
    *guild = !*guild;
  }
  ImGui::SameLine();
  if (BarButton("##map", ICON_FA_MAP, *map, "Map (M)")) {
    *map = !*map;
  }
  ImGui::SameLine();
  if (BarButton("##help", ICON_FA_CIRCLE_QUESTION, *help, "Help (H)")) {
    *help = !*help;
  }
  ImGui::SameLine();
  if (BarButton("##menu", ICON_FA_BARS, open_ || settings_, "Menu (Escape)")) {
    open_ = !open_;
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
}

}  // namespace nanommo
