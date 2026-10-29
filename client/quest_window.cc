#include "client/quest_window.h"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "client/icon.h"
#include "extras/IconsFontAwesome6.h"

namespace nanommo {

namespace {

constexpr ImVec4 kTitle{1.0f, 0.82f, 0.3f, 1.0f};
constexpr ImVec4 kDone{0.45f, 0.85f, 0.45f, 1.0f};

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
  ImGui::SetNextWindowSize({380.0f, 0.0f});
  bool open = true;
  ImGui::Begin(name_.c_str(), &open,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
  std::optional<protocol::Input> ask;
  for (const uint32_t id : quests_) {
    const quests::Quest& quest = content_.quests.at(id);
    ImGui::PushID(static_cast<int>(id));
    ImGui::TextColored(kTitle, "%s", quest.name().c_str());
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
      if (ImGui::Button("Take")) {
        ask.emplace();
        ask->mutable_take()->set_npc(*npc_);
        ask->mutable_take()->set_quest(id);
      }
    } else if (on->second < quest.kill().count()) {
      ImGui::TextDisabled("%u of %u killed", on->second, quest.kill().count());
    } else if (ImGui::Button("Turn in")) {
      ask.emplace();
      ask->mutable_turn_in()->set_npc(*npc_);
      ask->mutable_turn_in()->set_quest(id);
    }
    ImGui::PopID();
    ImGui::Separator();
  }
  ImGui::End();
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

void DrawQuestTracker(const Content& content, const protocol::Quests& yours) {
  if (yours.on().empty()) {
    return;
  }
  constexpr float kMargin = 10.0f;
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(
      {display.x - kMargin, ImGui::GetFrameHeight() + kMargin},
      ImGuiCond_Always, {1.0f, 0.0f});
  ImGui::SetNextWindowBgAlpha(0.4f);
  ImGui::Begin("Quests", nullptr,
               ImGuiWindowFlags_NoDecoration |
                   ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoFocusOnAppearing |
                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
  // In the order of their ids, as a protobuf map has none.
  const std::map<uint32_t, uint32_t> on(yours.on().begin(), yours.on().end());
  for (const auto& [id, kills] : on) {
    const auto quest = content.quests.find(id);
    if (quest == content.quests.end()) {
      continue;
    }
    ImGui::TextColored(kTitle, "%s", quest->second.name().c_str());
    const std::string wanted = Monsters(content, quest->second.kill());
    // Done, it says so, until it's turned in.
    if (kills >= quest->second.kill().count()) {
      ImGui::TextColored(kDone, "  %s  %u / %s", ICON_FA_CHECK, kills,
                         wanted.c_str());
    } else {
      ImGui::Text("  %u / %s", kills, wanted.c_str());
    }
  }
  ImGui::End();
}

}  // namespace nanommo
