// Each sound the game makes, to listen to one at a time, outside the game:
// up and down pick one, and Space plays a new take of it and draws it.
#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "client/audio.h"
#include "client/characters.h"
#include "client/foley.h"
#include "client/synth.h"
#include "client/window.h"
#include "content/content.h"
#include "terrain/lands.h"

namespace {

struct Entry {
  std::string name;
  /// A take of the sound, from its seed.
  std::function<nanommo::Samples(uint32_t)> make;
};

constexpr std::array<const char*, 7> kGroundNames = {
    "grass", "sand", "rock", "snow", "ash", "stone", "water"};

std::vector<Entry> Entries(const nanommo::SkillTable& skills) {
  std::vector<Entry> entries;
  entries.reserve(nanommo::kGrounds.size() + skills.by_id.size() + 1 +
                  nanommo::kLands.size());
  for (const nanommo::Ground ground : nanommo::kGrounds) {
    entries.push_back(
        {absl::StrCat("Step on ", kGroundNames[static_cast<int>(ground)]),
         [ground](uint32_t seed) { return nanommo::Step(ground, seed); }});
  }
  for (const auto& [id, skill] : skills.by_id) {
    const nanommo::skills::Look look = skill.look();
    // As cast at a monster, so that its fx at the target sound too.
    entries.push_back({skill.name(), [look](uint32_t seed) {
                         return nanommo::SkillSound(look, seed, true);
                       }});
  }
  entries.push_back({"Going through a gate",
                     [](uint32_t seed) { return nanommo::Departure(seed); }});
  for (int land = 0; land < static_cast<int>(nanommo::kLands.size()); ++land) {
    entries.push_back(
        {absl::StrCat("Arriving in the ", nanommo::kLands[land].name),
         [land](uint32_t) { return nanommo::Arrival(land); }});
  }
  return entries;
}

// The sound's shape: the highest and lowest sample under each column.
void DrawWave(const nanommo::Samples& samples, Rectangle box, Color color) {
  DrawRectangleLinesEx(box, 1.0f, DARKGRAY);
  const float middle = box.y + box.height / 2.0f;
  DrawLineV({box.x, middle}, {box.x + box.width, middle}, DARKGRAY);
  if (samples.empty()) {
    return;
  }
  const int columns = static_cast<int>(box.width);
  for (int x = 0; x < columns; ++x) {
    const std::size_t from = samples.size() * x / columns;
    const std::size_t to =
        std::max(samples.size() * (x + 1) / columns, from + 1);
    float low = 0.0f;
    float high = 0.0f;
    for (std::size_t i = from; i < to && i < samples.size(); ++i) {
      low = std::min(low, samples[i]);
      high = std::max(high, samples[i]);
    }
    DrawLineV({box.x + x, middle - high * box.height / 2.0f},
              {box.x + x, middle - low * box.height / 2.0f}, color);
  }
}

}  // namespace

int main() {
  const absl::StatusOr<nanommo::Content> content =
      nanommo::ReadContent(nanommo::ContentFolder());
  if (!content.ok()) {
    std::cerr << "nanommo sounds: " << content.status() << "\n";
    return 1;
  }
  absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("nanoMMO sounds", 960, 540);
  if (!window.ok()) {
    std::cerr << "nanommo sounds: " << window.status() << "\n";
    return 1;
  }
  absl::StatusOr<std::unique_ptr<nanommo::AudioDevice>> audio =
      nanommo::AudioDevice::Create();
  if (!audio.ok()) {
    std::cerr << "nanommo sounds: " << audio.status() << "\n";
  }

  const std::vector<Entry> entries = Entries(content->skills);
  int picked = 0;
  uint32_t seed = 0;
  nanommo::Samples shown;
  std::unique_ptr<nanommo::Clip> playing;
  while (!(*window)->ShouldClose()) {
    if (IsKeyPressed(KEY_DOWN)) {
      picked = (picked + 1) % static_cast<int>(entries.size());
    }
    if (IsKeyPressed(KEY_UP)) {
      picked = (picked + static_cast<int>(entries.size()) - 1) %
               static_cast<int>(entries.size());
    }
    // Each press is a take of its own.
    if (IsKeyPressed(KEY_SPACE)) {
      shown = entries[picked].make(++seed);
      if (audio.ok()) {
        playing = nanommo::Clip::Load(**audio, shown, 1);
        playing->Play(1.0f, 0.0f, 1.0f);
      }
    }

    BeginDrawing();
    ClearBackground({18, 20, 26, 255});
    for (std::size_t i = 0; i < entries.size(); ++i) {
      const int y = 20 + 22 * static_cast<int>(i);
      const bool here = static_cast<int>(i) == picked;
      if (here) {
        DrawRectangle(12, y - 3, 300, 22, {179, 18, 42, 255});
      }
      DrawText(entries[i].name.c_str(), 20, y, 18, here ? WHITE : LIGHTGRAY);
    }
    DrawWave(shown, {340.0f, 60.0f, 600.0f, 420.0f}, {255, 140, 90, 255});
    DrawText(absl::StrCat(entries[picked].name, ", take ", seed).c_str(), 340,
             20, 20, WHITE);
    DrawText(absl::StrCat(static_cast<float>(shown.size()) / nanommo::kRate,
                          " seconds")
                 .c_str(),
             340, 492, 18, GRAY);
    EndDrawing();
  }
  return 0;
}
