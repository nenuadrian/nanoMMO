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
#include "absl/strings/substitute.h"
#include "client/icon.h"
#include "client/names.h"
#include "client/theme.h"
#include "client/translations.h"
#include "extras/IconsFontAwesome6.h"

namespace nanommo {

namespace {

constexpr ImVec4 kTitle{1.0f, 0.82f, 0.3f, 1.0f};

// "3 Skeletons", by the monster's kind.
std::string Monsters(const Content& content, const quests::Kill& kill) {
  const auto kind = content.monsters.kinds.find(kill.monster());
  if (kind == content.monsters.kinds.end()) {
    return absl::Substitute(TrN("$0 monster", "$0 monsters", kill.count()),
                            kill.count());
  }
  return NameOf(kind->second, kill.count());
}

// The experience a quest gives, a plural, as the servers' is, in a quest's
// prizes.
std::string Experience(const quests::Quest& quest) {
  return absl::Substitute(
      TrN("$0 experience", "$0 experience", quest.experience()),
      quest.experience());
}

// What a quest gives, an item's icon and name and how many: "Health Potion
// x2".
std::string Prize(const items::Item& item, uint32_t count) {
  // Translators: An item's name, and how many of it.
  return absl::StrCat(Icon(item.look().icon()), "  ",
                      absl::Substitute(Tr("$0 x$1"), NameOf(item), count));
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
    ImGui::TextColored(kTitle, "%s", NameOf(quest));
    ImGui::PopFont();
    ImGui::TextWrapped("%s", TextOf(quest));
    ImGui::Spacing();
    ImGui::Text(
        "%s  %s", ICON_FA_SKULL,
        absl::Substitute(Tr("Kill $0"), Monsters(content_, quest.kill()))
            .c_str());
    ImGui::Text("%s  %s", ICON_FA_STAR, Experience(quest).c_str());
    for (const quests::Prize& prize : quest.prizes()) {
      ImGui::TextUnformatted(
          Prize(content_.items.by_id.at(prize.item()), prize.count()).c_str());
    }
    ImGui::Spacing();
    const auto on = yours.on().find(id);
    if (std::ranges::find(yours.done(), id) != yours.done().end()) {
      ImGui::TextDisabled("%s", Tr("Done"));
    } else if (on == yours.on().end()) {
      if (GameButton("Take the quest", {0.0f, 0.0f}, true)) {
        ask.emplace();
        ask->mutable_take()->set_npc(*npc_);
        ask->mutable_take()->set_quest(id);
      }
    } else if (on->second < quest.kill().count()) {
      ImGui::TextDisabled("%s",
                          absl::Substitute(Tr("$0 of $1 killed"), on->second,
                                           quest.kill().count())
                              .c_str());
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

std::optional<protocol::Input> QuestLog::Build(const protocol::Quests& yours) {
  if (!visible_) {
    return std::nullopt;
  }
  // In the order of their ids, as a protobuf map has none, and those they've
  // done, the highest id first.
  const std::map<uint32_t, uint32_t> on(yours.on().begin(), yours.on().end());
  const std::vector<uint32_t> done(yours.done().rbegin(), yours.done().rend());
  const auto known = [&](uint32_t id) { return content_.quests.contains(id); };
  // The one picked, while it's still on or done, or else the first on.
  if (!on.contains(picked_) && std::ranges::find(done, picked_) == done.end()) {
    picked_ = on.empty() ? 0 : on.begin()->first;
    giving_up_ = false;
  }

  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x / 2.0f, display.y * 0.45f},
                          ImGuiCond_FirstUseEver, {0.5f, 0.5f});
  ImGui::SetNextWindowSize(Ui(ImVec2{680.0f, 420.0f}));
  std::optional<protocol::Input> ask;
  if (!BeginPanel("Quests", &visible_,
                  ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                      ImGuiWindowFlags_NoCollapse)) {
    EndPanel();
    return ask;
  }

  // The quests, down the left.
  ImGui::BeginChild("List", {Ui(230.0f), 0.0f});
  ImGui::SeparatorText(Tr("On"));
  for (const auto& [id, kills] : on) {
    if (!known(id)) {
      continue;
    }
    const quests::Quest& quest = content_.quests.at(id);
    const bool finished = kills >= quest.kill().count();
    const std::string label = absl::StrCat(finished ? ICON_FA_CHECK "  " : "",
                                           NameOf(quest), "###quest", id);
    if (ImGui::Selectable(label.c_str(), picked_ == id)) {
      picked_ = id;
      giving_up_ = false;
    }
  }
  if (on.empty()) {
    ImGui::TextDisabled("%s", Tr("None yet."));
  }
  if (!done.empty()) {
    ImGui::Spacing();
    ImGui::SeparatorText(Tr("Done"));
    for (const uint32_t id : done) {
      if (!known(id)) {
        continue;
      }
      ImGui::PushStyleColor(ImGuiCol_Text,
                            ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
      const std::string label =
          absl::StrCat(NameOf(content_.quests.at(id)), "###done", id);
      if (ImGui::Selectable(label.c_str(), picked_ == id)) {
        picked_ = id;
        giving_up_ = false;
      }
      ImGui::PopStyleColor();
    }
  }
  ImGui::EndChild();
  ImGui::SameLine(0.0f, Ui(16.0f));

  // The one picked, on the right.
  ImGui::BeginChild("Quest");
  if (!known(picked_)) {
    ImGui::TextDisabled("%s",
                        Tr("Someone with a ! over them has a quest for you."));
    ImGui::EndChild();
    EndPanel();
    return ask;
  }
  const quests::Quest& quest = content_.quests.at(picked_);
  const auto giver = content_.npcs.kinds.find(quest.giver());
  const char* from = giver != content_.npcs.kinds.end() ? NameOf(giver->second)
                                                        : Tr("someone");
  ImGui::PushFont(GetFonts().title, 19.0f);
  ImGui::TextColored(kTitle, "%s", NameOf(quest));
  ImGui::PopFont();
  ImGui::TextDisabled("%s", absl::Substitute(Tr("From $0"), from).c_str());
  ImGui::Spacing();
  ImGui::TextWrapped("%s", TextOf(quest));
  ImGui::Spacing();
  ImGui::Text("%s  %s", ICON_FA_SKULL,
              absl::Substitute(Tr("Kill $0"), Monsters(content_, quest.kill()))
                  .c_str());
  const auto kills = on.find(picked_);
  if (kills != on.end()) {
    // How far they've got, as a bar.
    const uint32_t wanted = std::max(quest.kill().count(), 1u);
    const uint32_t got = std::min(kills->second, wanted);
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const std::string count = absl::StrCat(got, " / ", wanted);
    DrawBar(ImGui::GetWindowDrawList(), at, {at.x + width, at.y + Ui(18.0f)},
            static_cast<float>(got) / static_cast<float>(wanted),
            colors::kGoldDark, IM_COL32(90, 58, 10, 255), count.c_str());
    ImGui::Dummy({width, Ui(22.0f)});
  }
  ImGui::Text("%s  %s", ICON_FA_STAR, Experience(quest).c_str());
  for (const quests::Prize& prize : quest.prizes()) {
    const auto item = content_.items.by_id.find(prize.item());
    if (item == content_.items.by_id.end()) {
      continue;
    }
    ImGui::TextUnformatted(Prize(item->second, prize.count()).c_str());
  }
  ImGui::Spacing();
  if (kills == on.end()) {
    ImGui::TextDisabled("%s", Tr("Turned in."));
  } else {
    if (kills->second >= quest.kill().count()) {
      ImGui::TextColored(
          ImGui::ColorConvertU32ToFloat4(colors::kGood), ICON_FA_CHECK "  %s",
          absl::Substitute(Tr("Done: take it back to $0."), from).c_str());
    }
    ImGui::Spacing();
    if (!giving_up_) {
      giving_up_ = GameButton("Give up", {Ui(160.0f), 0.0f});
    } else {
      ImGui::TextColored(
          {1.0f, 0.7f, 0.4f, 1.0f}, "%s",
          absl::Substitute(
              Tr("Give up $0? What you've killed for it goes too."),
              NameOf(quest))
              .c_str());
      if (GameButton("Give up", {Ui(160.0f), 0.0f}, true)) {
        ask.emplace();
        ask->set_give_up(picked_);
        giving_up_ = false;
      }
      ImGui::SameLine();
      if (GameButton("Keep it", {Ui(160.0f), 0.0f})) {
        giving_up_ = false;
      }
    }
  }
  ImGui::EndChild();
  EndPanel();
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
  line(fonts.title, Ui(16.0f), colors::kGold, Tr("Quests"));
  y += Ui(4.0f);
  // In the order of their ids, as a protobuf map has none.
  const std::map<uint32_t, uint32_t> on(yours.on().begin(), yours.on().end());
  for (const auto& [id, kills] : on) {
    const auto quest = content.quests.find(id);
    if (quest == content.quests.end()) {
      continue;
    }
    line(fonts.bold, Ui(17.0f), colors::kGoldLight, NameOf(quest->second));
    const std::string wanted = Monsters(content, quest->second.kill());
    // Done, it says so, until it's turned in.
    if (kills >= quest->second.kill().count()) {
      // Translators: How many they've killed, of so many of a kind of
      // monster, such as 3 Skeletons.
      line(fonts.text, Ui(16.0f), colors::kGood,
           absl::StrCat(ICON_FA_CHECK " ",
                        absl::Substitute(Tr("$0 / $1, done"), kills, wanted)));
    } else {
      line(fonts.text, Ui(16.0f), colors::kPaper,
           absl::StrCat(kills, " / ", wanted));
    }
    y += Ui(6.0f);
  }
}

}  // namespace nanommo
