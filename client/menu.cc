#include "client/menu.h"

#include <imgui.h>

#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <utility>

#include "client/theme.h"
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

// The keys, and what each does, for the keys' page.
constexpr std::array<std::pair<const char*, const char*>, 17> kKeys = {{
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
    {"G", "Your guild"},
    {"Enter", "Chat"},
    {"Enter, then /g and a space", "Talk to your guild"},
    {"Escape", "Close what's open, or this menu"},
    {"Drag an item out of the bag", "Throw it away"},
}};

// A button in the bottom right row, with an icon, square.
bool BarButton(const char* id, const char* icon, bool on, const char* tip) {
  const float size = Ui(38.0f);
  const bool clicked = ImGui::InvisibleButton(id, {size, size});
  const bool hovered = ImGui::IsItemHovered();
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  DrawSlot(draw, min, max, on ? colors::kEmber : (hovered ? colors::kGold : 0));
  ImFont* font = GetFonts().text;
  const float icon_size = Ui(18.0f);
  const ImVec2 extent = font->CalcTextSizeA(icon_size, FLT_MAX, 0.0f, icon);
  DrawShadowedText(
      draw, font, icon_size,
      {(min.x + max.x - extent.x) / 2.0f, (min.y + max.y - extent.y) / 2.0f},
      hovered || on ? colors::kGoldLight : colors::kPaper, icon);
  if (hovered) {
    ImGui::SetTooltip("%s", tip);
  }
  return clicked;
}

}  // namespace

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

  // The pages down the left, and the one picked on the right.
  constexpr std::array<std::pair<Page, const char*>, 4> kPages = {{
      {Page::kInterface, ICON_FA_TABLE_COLUMNS "  Interface"},
      {Page::kSound, ICON_FA_VOLUME_HIGH "  Sound"},
      {Page::kGraphics, ICON_FA_DISPLAY "  Graphics"},
      {Page::kKeys, ICON_FA_KEYBOARD "  Keys"},
  }};
  ImGui::BeginChild("Pages", {Ui(170.0f), 0.0f});
  for (const auto& [page, name] : kPages) {
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, {0.0f, 0.5f});
    if (ImGui::Selectable(name, page_ == page, 0, {0.0f, Ui(34.0f)})) {
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
      ImGui::SeparatorText("Interface");
      if (!ImGui::IsAnyItemActive() || scale_ == 0.0f) {
        scale_ = settings.ui.scale;
      }
      int percent = static_cast<int>(std::lround(scale_ * 100.0f));
      if (ImGui::SliderInt("Size", &percent, 75, 150, "%d%%",
                           ImGuiSliderFlags_AlwaysClamp)) {
        scale_ = static_cast<float>(percent) / 100.0f;
      }
      if (ImGui::IsItemDeactivatedAfterEdit()) {
        settings.ui.scale = scale_;
      }
      ImGui::TextDisabled(
          "On top of its size for your screen, which grows with it.");
      ImGui::Spacing();
      ImGui::Checkbox("Turn the minimap with the camera",
                      &settings.ui.minimap_turns);
      ImGui::Spacing();
      if (GameButton("Put the hotbar back in order", {0.0f, 0.0f})) {
        settings.ui.hotbar.clear();
        *reset_hotbar = true;
      }
      ImGui::TextDisabled("Drag a skill onto another to swap them.");
      break;
    }
    case Page::kSound:
      ImGui::SeparatorText("Sound");
      SliderPercent("Volume", settings.sound.master);
      SliderPercent("Music", settings.sound.music);
      SliderPercent("Effects", settings.sound.effects);
      ImGui::Checkbox("Mute", &settings.sound.muted);
      ImGui::Spacing();
      ImGui::TextDisabled(ICON_FA_MUSIC
                          " \"Background Music\" by Pavel Bekirov, from "
                          "Pixabay");
      break;
    case Page::kGraphics:
      ImGui::SeparatorText("Graphics");
      ImGui::Checkbox("Bloom", &settings.graphics.bloom);
      ImGui::Checkbox("Cel shading", &settings.graphics.cel_shading);
      ImGui::Checkbox("Anti-aliasing", &settings.graphics.antialias);
      ImGui::SeparatorText(ICON_FA_CLOUD_SUN "  Sky");
      ImGui::Checkbox("Weather", &settings.graphics.weather);
      ImGui::Checkbox("Day and night", &settings.graphics.day_night);
      break;
    case Page::kKeys:
      ImGui::SeparatorText("Keys");
      if (ImGui::BeginTable("Keys", 2, ImGuiTableFlags_RowBg)) {
        for (const auto& [key, does] : kKeys) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::PushFont(GetFonts().bold, 0.0f);
          ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(colors::kGold),
                             "%s", key);
          ImGui::PopFont();
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(does);
        }
        ImGui::EndTable();
      }
      break;
  }
  ImGui::PopItemWidth();
  ImGui::EndChild();
  EndPanel();
}

void Menu::BuildButtons(bool* character, bool* bag, bool* guild, bool* map) {
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x - Ui(18.0f), display.y - Ui(18.0f)},
                          ImGuiCond_Always, {1.0f, 1.0f});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Ui(ImVec2{10.0f, 10.0f}));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Ui(ImVec2{6.0f, 6.0f}));
  ImGui::Begin("Buttons", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav |
                   ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoFocusOnAppearing |
                   ImGuiWindowFlags_NoSavedSettings);
  const ImVec2 min = ImGui::GetWindowPos();
  const ImVec2 size = ImGui::GetWindowSize();
  DrawPanel(ImGui::GetWindowDrawList(), min, {min.x + size.x, min.y + size.y},
            0.9f);
  if (BarButton("##character", ICON_FA_USER, *character, "Character (C)")) {
    *character = !*character;
  }
  ImGui::SameLine();
  if (BarButton("##bag", ICON_FA_BAG_SHOPPING, *bag, "Bag (I)")) {
    *bag = !*bag;
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
  if (BarButton("##menu", ICON_FA_BARS, open_ || settings_, "Menu (Escape)")) {
    open_ = !open_;
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
}

}  // namespace nanommo
