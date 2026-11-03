#include "client/quest_window.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "client/icon.h"
#include "client/theme.h"
#include "extras/IconsFontAwesome6.h"

namespace nanommo {

namespace {

constexpr ImVec4 kTitle{1.0f, 0.82f, 0.3f, 1.0f};

// "3 Skeletons", by the monster's kind.
std::string Monsters(const Content& content, const quests::Kill& kill) {
  const auto kind = content.monsters.kinds.find(kill.monster());
  const std::string name =
      kind != content.monsters.kinds.end() ? kind->second.name() : "monster";
  return std::to_string(kill.count()) + " " + name + "s";
}

}  // namespace

void QuestWindow::Open(uint32_t npc, std::string name,
                       std::vector<uint32_t> quests) {
  npc_ = npc;
  name_ = std::move(name);
  quests_ = std::move(quests);
}

std::optional<protocol::Input> QuestWindow::Build(
    const protocol::Quests& yours) {
  if (!npc_) {
    return std::nullopt;
  }
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x / 2, display.y * 0.6f},
                          ImGuiCond_Appearing, {0.5f, 0.5f});
  ImGui::SetNextWindowSize({Ui(400.0f), 0.0f});
  bool open = true;
  BeginPanel(name_.c_str(), &open,
             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
  std::optional<protocol::Input> ask;
  for (const uint32_t id : quests_) {
    const quests::Quest& quest = content_.quests.at(id);
    ImGui::PushID(static_cast<int>(id));
    ImGui::PushFont(GetFonts().title, 17.0f);
    ImGui::TextColored(kTitle, "%s", quest.name().c_str());
    ImGui::PopFont();
    ImGui::TextWrapped("%s", quest.text().c_str());
    ImGui::Spacing();
    ImGui::Text("%s  Kill %s", ICON_FA_SKULL,
                Monsters(content_, quest.kill()).c_str());
    ImGui::Text("%s  %u experience", ICON_FA_STAR, quest.experience());
    for (const quests::Prize& prize : quest.prizes()) {
      const items::Item& item = content_.items.by_id.at(prize.item());
      ImGui::Text("%s  %s x%u", Icon(item.look().icon()).c_str(),
                  item.name().c_str(), prize.count());
    }
    ImGui::Spacing();
    const auto on = yours.on().find(id);
    if (std::ranges::find(yours.done(), id) != yours.done().end()) {
      ImGui::TextDisabled("Done");
    } else if (on == yours.on().end()) {
      if (GameButton("Take the quest", {0.0f, 0.0f}, true)) {
        ask.emplace();
        ask->mutable_take()->set_npc(*npc_);
        ask->mutable_take()->set_quest(id);
      }
    } else if (on->second < quest.kill().count()) {
      ImGui::TextDisabled("%u of %u killed", on->second, quest.kill().count());
    } else if (GameButton("Turn it in", {0.0f, 0.0f}, true)) {
      ask.emplace();
      ask->mutable_turn_in()->set_npc(*npc_);
      ask->mutable_turn_in()->set_quest(id);
    }
    ImGui::PopID();
    ImGui::Spacing();
  }
  EndPanel();
  if (!open) {
    npc_.reset();
  }
  return ask;
}

void MarkQuestGivers(std::vector<Figure>& figures, const Content& content,
                     const protocol::Quests& yours) {
  for (Figure& figure : figures) {
    figure.mark = 0;
    for (const uint32_t id : figure.quests) {
      const auto on = yours.on().find(id);
      if (on != yours.on().end()) {
        const auto quest = content.quests.find(id);
        if (quest != content.quests.end() &&
            on->second >= quest->second.kill().count()) {
          figure.mark = '?';
        }
      } else if (figure.mark == 0 &&
                 std::ranges::find(yours.done(), id) == yours.done().end()) {
        figure.mark = '!';
      }
    }
  }
}

void DrawQuestTracker(const Content& content, const protocol::Quests& yours,
                      float top) {
  if (yours.on().empty()) {
    return;
  }
  // Words with shadows, without a window, lined up on the right, so that
  // they read on any ground, and hide as little of it as they can.
  ImDrawList* draw = ImGui::GetBackgroundDrawList();
  const Fonts& fonts = GetFonts();
  const float right = ImGui::GetIO().DisplaySize.x - Ui(24.0f);
  float y = top + Ui(10.0f);
  const auto line = [&](ImFont* font, float size, ImU32 color,
                        const std::string& text) {
    const ImVec2 extent =
        font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
    DrawShadowedText(draw, font, size, {right - extent.x, y}, color,
                     text.c_str());
    y += extent.y + Ui(2.0f);
  };
  line(fonts.title, Ui(16.0f), colors::kGold, "Quests");
  y += Ui(4.0f);
  // In the order of their ids, as a protobuf map has none.
  const std::map<uint32_t, uint32_t> on(yours.on().begin(), yours.on().end());
  for (const auto& [id, kills] : on) {
    const auto quest = content.quests.find(id);
    if (quest == content.quests.end()) {
      continue;
    }
    line(fonts.bold, Ui(17.0f), colors::kGoldLight, quest->second.name());
    const std::string wanted = Monsters(content, quest->second.kill());
    // Done, it says so, until it's turned in.
    if (kills >= quest->second.kill().count()) {
      line(fonts.text, Ui(16.0f), colors::kGood,
           absl::StrCat(ICON_FA_CHECK " ", kills, " / ", wanted, ", done"));
    } else {
      line(fonts.text, Ui(16.0f), colors::kPaper,
           absl::StrCat(kills, " / ", wanted));
    }
    y += Ui(6.0f);
  }
}

}  // namespace nanommo
