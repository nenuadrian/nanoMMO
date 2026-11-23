#include "client/herald_window.h"

#include <imgui.h>
#include <raylib.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/substitute.h"
#include "client/names.h"
#include "client/theme.h"
#include "client/translations.h"

namespace nanommo {

void HeraldWindow::Open(uint32_t npc, std::string name, uint32_t faction) {
  npc_ = npc;
  name_ = std::move(name);
  faction_ = faction;
  asking_ = false;
}

void HeraldWindow::Close() {
  npc_.reset();
  asking_ = false;
}

std::optional<uint32_t> HeraldWindow::Build(
    const std::vector<Figure>& figures) {
  const auto theirs = factions_.find(faction_);
  if (!npc_ || theirs == factions_.end()) {
    return std::nullopt;
  }
  const auto you = std::ranges::find_if(figures, &Figure::you);
  const auto yours =
      factions_.find(you != figures.end() ? you->faction : uint32_t{0});
  const char* faction = NameOf(theirs->second);
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x / 2, display.y * 0.7f},
                          ImGuiCond_Appearing, {0.5f, 0.5f});
  ImGui::SetNextWindowSize({Ui(380.0f), 0.0f});
  bool open = true;
  BeginPanel(name_.c_str(), &open,
             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
  // The faction's name, in its colour, as it goes by its players' names.
  const Color color = GetColor(theirs->second.color());
  ImGui::PushFont(GetFonts().title, 22.0f);
  ImGui::TextColored(
      ImGui::ColorConvertU32ToFloat4(IM_COL32(color.r, color.g, color.b, 255)),
      "%s", faction);
  ImGui::PopFont();
  ImGui::Separator();
  std::optional<uint32_t> sworn;
  if (yours != factions_.end()) {
    // Theirs or another, it's for good.
    ImGui::TextWrapped("%s", absl::Substitute(Tr("You're sworn to $0 already."),
                                              NameOf(yours->second))
                                 .c_str());
  } else if (!asking_) {
    ImGui::TextWrapped(
        "%s",
        absl::Substitute(Tr("Swear to $0, and its colour goes by your name, "
                            "for everyone to see."),
                         faction)
            .c_str());
    if (GameButton("Enlist", {-1.0f, 0.0f}, true)) {
      asking_ = true;
    }
  } else {
    ImGui::TextWrapped(
        "%s", absl::Substitute(Tr("It's for good: there's no leaving $0, nor "
                                  "swearing to another."),
                               faction)
                  .c_str());
    if (GameButton("Swear", {Ui(170.0f), 0.0f}, true)) {
      sworn = npc_;
      open = false;
    }
    ImGui::SameLine();
    if (GameButton("Not now", {Ui(170.0f), 0.0f})) {
      open = false;
    }
  }
  EndPanel();
  if (!open) {
    Close();
  }
  return sworn;
}

}  // namespace nanommo
