#include "client/trainer_window.h"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "client/icon.h"

namespace nanommo {

namespace {

// In points from the window's left, so that the buttons line up whatever the
// skills' names.
constexpr float kButtons = 200.0f;

}  // namespace

void TrainerWindow::Open(uint32_t npc, std::string name,
                         std::vector<uint32_t> teaches) {
  npc_ = npc;
  name_ = std::move(name);
  teaches_ = std::move(teaches);
}

std::optional<protocol::Learn> TrainerWindow::Build(
    const std::vector<Figure>& figures, const protocol::World& world) {
  if (!npc_) {
    return std::nullopt;
  }
  const auto you = std::ranges::find_if(figures, &Figure::you);
  const int level = you != figures.end() ? you->level : 0;
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos({display.x / 2, display.y * 0.7f},
                          ImGuiCond_Appearing, {0.5f, 0.5f});
  ImGui::SetNextWindowSize({320.0f, 0.0f});
  bool open = true;
  ImGui::Begin(name_.c_str(), &open,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
  ImGui::TextWrapped("I can teach you these, once you're ready for them.");
  ImGui::Separator();
  std::optional<protocol::Learn> learn;
  for (const uint32_t id : teaches_) {
    const skills::Skill& skill = skills_.by_id.at(id);
    ImGui::PushID(static_cast<int>(id));
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s  %s", Icon(skill.look().icon()).c_str(),
                skill.name().c_str());
    ImGui::SameLine(kButtons);
    if (std::ranges::find(world.skills(), id) != world.skills().end()) {
      ImGui::TextDisabled("Known");
    } else if (level < static_cast<int>(skill.level())) {
      ImGui::TextDisabled("From level %u", skill.level());
    } else if (ImGui::Button("Learn")) {
      learn.emplace();
      learn->set_npc(*npc_);
      learn->set_skill(id);
    }
    ImGui::PopID();
  }
  ImGui::End();
  if (!open) {
    npc_.reset();
  }
  return learn;
}

}  // namespace nanommo
