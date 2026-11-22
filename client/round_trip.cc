#include "client/round_trip.h"

#include <imgui.h>

#include <cfloat>
#include <cmath>
#include <iterator>
#include <string>

#include "absl/strings/substitute.h"
#include "client/theme.h"
#include "client/translations.h"

namespace nanommo {

namespace {

// How much each new round trip moves the one shown, so that it doesn't jump
// with each tick.
constexpr float kSmoothing = 0.1f;

}  // namespace

void RoundTrip::Sent(uint32_t sequence, double now) { sent_[sequence] = now; }

void RoundTrip::Acked(uint32_t acked, double now) {
  const auto sent = sent_.find(acked);
  if (sent == sent_.end()) {
    return;
  }
  const float measured = static_cast<float>((now - sent->second) * 1000.0);
  milliseconds_ = milliseconds_ == 0.0f
                      ? measured
                      : milliseconds_ + (measured - milliseconds_) * kSmoothing;
  // Those before it won't be acked on their own any more.
  sent_.erase(sent_.begin(), std::next(sent));
}

float DrawRoundTrip(float milliseconds, float top) {
  if (milliseconds == 0.0f) {
    return 0.0f;
  }
  const std::string text = absl::Substitute(
      Tr("$0 ms"), static_cast<int>(std::lround(milliseconds)));
  ImFont* font = GetFonts().bold;
  const float size = Ui(13.0f);
  const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
  const float right = ImGui::GetIO().DisplaySize.x - Ui(24.0f);
  DrawShadowedText(ImGui::GetBackgroundDrawList(), font, size,
                   {right - extent.x, top + Ui(6.0f)}, colors::kMuted,
                   text.c_str());
  return extent.y + Ui(6.0f);
}

}  // namespace nanommo
