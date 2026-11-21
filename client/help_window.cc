#include "client/help_window.h"

#include <imgui.h>

#include <array>
#include <utility>

#include "client/menu.h"
#include "client/theme.h"
#include "extras/IconsFontAwesome6.h"

namespace nanommo {

namespace {

// A line of a page, after a dot, wrapped to the page.
void Line(const char* text) {
  ImGui::Bullet();
  ImGui::TextWrapped("%s", text);
}

}  // namespace

void HelpWindow::Build() {
  if (!visible_) {
    return;
  }
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x / 2.0f, display.y * 0.45f},
                          ImGuiCond_FirstUseEver, {0.5f, 0.5f});
  ImGui::SetNextWindowSize(Ui(ImVec2{720.0f, 460.0f}));
  if (!BeginPanel("Help", &visible_,
                  ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                      ImGuiWindowFlags_NoCollapse)) {
    EndPanel();
    return;
  }
  static constexpr std::array<std::pair<Page, const char*>, 7> kPages = {{
      {Page::kMoving, ICON_FA_PERSON_WALKING "  Moving"},
      {Page::kFighting, ICON_FA_HAND_FIST "  Fighting"},
      {Page::kItems, ICON_FA_BAG_SHOPPING "  Items"},
      {Page::kQuests, ICON_FA_SCROLL "  Quests"},
      {Page::kGuilds, ICON_FA_SHIELD_HALVED "  Guilds and duels"},
      {Page::kChat, ICON_FA_COMMENTS "  Chat"},
      {Page::kKeys, ICON_FA_KEYBOARD "  Keys"},
  }};
  ImGui::BeginChild("Pages", {Ui(190.0f), 0.0f});
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
  switch (page_) {
    case Page::kMoving:
      ImGui::SeparatorText("Moving");
      Line("W A S D, or the arrows, walk, and Shift runs.");
      Line(
          "Double-click the ground, or click a place on the map (M), and you "
          "walk there, round what's in the way.");
      Line(
          "Q and E, or the right button held down, turn the camera, and the "
          "mouse wheel zooms in and out.");
      Line("A gate takes you to another land.");
      break;
    case Page::kFighting:
      ImGui::SeparatorText("Fighting");
      Line(
          "Click a monster to pick it. Its name and health show over it, and "
          "in the frame at the top.");
      Line(
          "Space swings what you hold at the monster you picked. Too far "
          "away, you run up to it first.");
      Line(
          "1 to 9 use the skills on the hotbar, and with Shift, those on its "
          "second row. Trainers teach you more of them.");
      Line(
          "A monster's attack shows on the ground before it lands, filling "
          "as it comes: step out of its shape.");
      Line("When you die, wait for the count, then click Respawn.");
      break;
    case Page::kItems:
      ImGui::SeparatorText("Items");
      Line("What a monster drops goes into your bag (I).");
      Line(
          "Double-click an item to use it, wear it, or take it off. Or drag "
          "it onto your character (C).");
      Line("Drag an item out of the bag to throw it away.");
      Line(
          "Crafting (K) makes something from what's in your bag, over a few "
          "seconds. Moving, or being hit, stops it, and nothing's lost.");
      Line("To trade with someone, type /trade and their name in the chat.");
      break;
    case Page::kQuests:
      ImGui::SeparatorText("Quests");
      Line(
          "Someone with a ! over them has a quest for you. Click them to "
          "take it.");
      Line(
          "A ? over them means you've done it: click them again to turn it "
          "in, for your prize.");
      Line("The quests you're on show in the top right, under the minimap.");
      break;
    case Page::kGuilds:
      ImGui::SeparatorText("Guilds and duels");
      Line(
          "G opens your guild: found one, ask people in, and see who's "
          "online.");
      Line("To ask someone to a duel, type /duel and their name in the chat.");
      Line(
          "A guild's leader can type /war and another guild's name, to ask "
          "it to war for 30 minutes. Its players' names are red, and you can "
          "fight them anywhere.");
      break;
    case Page::kChat:
      ImGui::SeparatorText("Chat");
      Line("Enter opens the chat, and Enter again sends what you typed.");
      Line("/g and a space first talks to your guild.");
      Line(
          "/trade, /duel or /war and a name asks for a trade, a duel or a "
          "war.");
      break;
    case Page::kKeys:
      ImGui::SeparatorText("Keys");
      BuildKeys();
      break;
  }
  ImGui::EndChild();
  EndPanel();
}

}  // namespace nanommo
