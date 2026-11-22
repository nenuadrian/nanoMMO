#include "client/boss_bar.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
#include "client/theme.h"
#include "client/translations.h"

namespace nanommo {

namespace {

// In points, at a scale of one: from the top of the screen, under the
// target's frame, and the bar's size.
constexpr float kTop = 146.0f;
constexpr float kWidth = 520.0f;
constexpr float kHeight = 18.0f;

}  // namespace

const Figure* ShownBoss(const std::vector<Figure>& figures, uint32_t target) {
  const auto you = std::ranges::find_if(figures, &Figure::you);
  const Figure* shown = nullptr;
  float nearest = kBossBarReach;
  for (const Figure& figure : figures) {
    if (!figure.monster || figure.phase == 0 ||
        figure.to == protocol::MOTION_DEAD) {
      continue;
    }
    if (figure.id == target) {
      return &figure;
    }
    if (you == figures.end()) {
      continue;
    }
    const float away = std::hypot(figure.position.x - you->position.x,
                                  figure.position.z - you->position.z);
    if (away <= nearest) {
      shown = &figure;
      nearest = away;
    }
  }
  return shown;
}

void DrawBossBar(const Figure& boss, const MonsterTable& monsters) {
  ImDrawList* draw = ImGui::GetBackgroundDrawList();
  const float width = Ui(kWidth);
  const float left = (ImGui::GetIO().DisplaySize.x - width) / 2.0f;
  const float top = Ui(kTop);
  DrawTitle(draw, {left + width / 2.0f, top}, Ui(20.0f), boss.name.c_str());

  const ImVec2 min{left, top + Ui(28.0f)};
  const ImVec2 max{left + width, min.y + Ui(kHeight)};
  const float share = boss.max_health > 0
                          ? static_cast<float>(boss.health) /
                                static_cast<float>(boss.max_health)
                          : 0.0f;
  const std::string health = absl::StrCat(boss.health, " / ", boss.max_health);
  DrawBar(draw, min, max, share, colors::kHealth, colors::kHealthDark,
          health.c_str());

  // Where each phase after the first starts, so that everyone can see the
  // next coming.
  int phases = static_cast<int>(boss.phase);
  if (const auto kind = monsters.kinds.find(boss.kind);
      kind != monsters.kinds.end()) {
    phases = kind->second.phases_size();
    for (int i = 1; i < phases; ++i) {
      const float from =
          static_cast<float>(kind->second.phases(i).health()) / 100.0f;
      const float x = left + width * from;
      draw->AddLine({x, min.y - Ui(3.0f)}, {x, max.y + Ui(3.0f)},
                    colors::kGoldLight, std::max(1.0f, Ui(2.0f)));
    }
  }

  const std::string phase =
      absl::Substitute(Tr("Phase $0 of $1"), boss.phase, phases);
  ImFont* bold = GetFonts().bold;
  const ImVec2 extent =
      bold->CalcTextSizeA(Ui(13.0f), FLT_MAX, 0.0f, phase.c_str());
  DrawShadowedText(draw, bold, Ui(13.0f),
                   {left + (width - extent.x) / 2.0f, max.y + Ui(5.0f)},
                   colors::kMuted, phase.c_str());
}

}  // namespace nanommo
