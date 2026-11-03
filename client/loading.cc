#include "client/loading.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <string>

#include "client/theme.h"

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
  // The words stay a little longer than the sky behind them, on a dark band
  // across the middle, so that they read on a light sky and a dark one.
  const float alpha = std::clamp(cover * 1.5f, 0.0f, 1.0f);
  const float middle = screen.y * 0.45f;
  const float band = Ui(110.0f);
  const ImU32 dark = Faded(colors::kNight, 0.7f * alpha);
  const ImU32 clear = Faded(colors::kNight, 0.0f);
  draw->AddRectFilledMultiColor({0.0f, middle - band}, {screen.x, middle},
                                clear, clear, dark, dark);
  draw->AddRectFilledMultiColor({0.0f, middle}, {screen.x, middle + band}, dark,
                                dark, clear, clear);
  ImFont* title_font = GetFonts().title;
  const float title_size = Ui(58.0f);
  const ImVec2 title_extent =
      title_font->CalcTextSizeA(title_size, FLT_MAX, 0.0f, title.c_str());
  const float top = middle - title_extent.y * 0.75f;
  DrawGradientText(draw, title_font, title_size,
                   {(screen.x - title_extent.x) / 2.0f, top},
                   Faded(colors::kGoldLight, alpha),
                   Faded(colors::kGoldDark, alpha), title.c_str());
  if (alpha >= 0.99f) {
    DrawDivider(draw,
                {screen.x / 2.0f - title_extent.x * 0.6f,
                 top + title_extent.y + Ui(4.0f)},
                title_extent.x * 1.2f);
  }
  ImFont* text = GetFonts().text;
  const float line_size = Ui(20.0f);
  const ImVec2 line_extent =
      text->CalcTextSizeA(line_size, FLT_MAX, 0.0f, line.c_str());
  DrawShadowedText(
      draw, text, line_size,
      {(screen.x - line_extent.x) / 2.0f, top + title_extent.y + Ui(14.0f)},
      Faded(colors::kPaper, alpha * 0.85f), line.c_str());
}

}  // namespace nanommo
