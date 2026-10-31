#include "client/loading.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <string>

namespace nanommo {

namespace {

// A frame that took long, such as the one that built the land, counts as a
// short one, so that the fog doesn't jump.
constexpr float kLongestFrame = 1.0f / 30.0f;

// Fast at first, then slower as it ends.
float EaseOut(float share) {
  const float left = 1.0f - std::clamp(share, 0.0f, 1.0f);
  return 1.0f - left * left * left;
}

}  // namespace

void Loading::Cover() {
  if (stage_ != Stage::kCovered) {
    stage_ = Stage::kCovering;
  }
  time_ = 0.0f;
}

void Loading::Lift() {
  stage_ = Stage::kLifting;
  time_ = 0.0f;
}

void Loading::Update(float seconds) {
  switch (stage_) {
    case Stage::kCovering:
      stage_ = Stage::kCovered;
      break;
    case Stage::kLifting:
      time_ += std::min(seconds, kLongestFrame);
      if (time_ >= kLift) {
        stage_ = Stage::kNone;
      }
      break;
    case Stage::kNone:
    case Stage::kCovered:
      break;
  }
}

float Loading::cover() const {
  switch (stage_) {
    case Stage::kCovering:
    case Stage::kCovered:
      return 1.0f;
    case Stage::kLifting:
      return 1.0f - std::clamp(time_ / kFade, 0.0f, 1.0f);
    case Stage::kNone:
      break;
  }
  return 0.0f;
}

float Loading::clear() const {
  switch (stage_) {
    case Stage::kCovering:
    case Stage::kCovered:
      return 0.0f;
    case Stage::kLifting:
      return EaseOut(time_ / kLift);
    case Stage::kNone:
      break;
  }
  return 1.0f;
}

void DrawLoadingScreen(float cover, const std::array<float, 3>& sky,
                       const std::string& title, const std::string& line) {
  if (cover <= 0.0f) {
    return;
  }
  ImDrawList* draw = ImGui::GetForegroundDrawList();
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  const auto byte = [](float value) {
    return static_cast<int>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
  };
  draw->AddRectFilled(
      {0.0f, 0.0f}, screen,
      IM_COL32(byte(sky[0]), byte(sky[1]), byte(sky[2]), byte(cover)));
  // Dark words on a light sky, such as the Meadows', and light on a dark one.
  const float light = 0.3f * sky[0] + 0.59f * sky[1] + 0.11f * sky[2];
  const int ink = light > 0.5f ? 30 : 235;
  // The words stay a little longer than the sky behind them.
  const int alpha = byte(cover * 1.5f);
  ImFont* font = ImGui::GetFont();
  const float title_size = ImGui::GetFontSize() * 3.2f;
  const float line_size = ImGui::GetFontSize() * 1.3f;
  const ImVec2 title_extent =
      font->CalcTextSizeA(title_size, FLT_MAX, 0.0f, title.c_str());
  const ImVec2 line_extent =
      font->CalcTextSizeA(line_size, FLT_MAX, 0.0f, line.c_str());
  const float top = screen.y * 0.42f - title_extent.y / 2.0f;
  draw->AddText(font, title_size, {(screen.x - title_extent.x) / 2.0f, top},
                IM_COL32(ink, ink, ink, alpha), title.c_str());
  draw->AddText(font, line_size,
                {(screen.x - line_extent.x) / 2.0f,
                 top + title_extent.y + line_size * 0.4f},
                IM_COL32(ink, ink, ink, alpha * 3 / 4), line.c_str());
}

}  // namespace nanommo
