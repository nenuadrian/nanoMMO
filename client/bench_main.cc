#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "client/atmosphere.h"
#include "client/camera.h"
#include "client/renderer.h"
#include "client/window.h"
#include "terrain/terrain.h"

// Walks once around the middle of the island, through the woods, and draws
// the same frames each time, as fast as it can.
int main(int argc, char* argv[]) {
  int frames = 1200;
  uint32_t seed = 1;
  if ((argc > 1 && !absl::SimpleAtoi(argv[1], &frames)) || frames < 1 ||
      (argc > 2 && !absl::SimpleAtoi(argv[2], &seed))) {
    std::cerr << "usage: nanommo_client_bench [frames] [seed] [results.json]\n";
    return 1;
  }
  absl::StatusOr<std::unique_ptr<nanommo::Window>> window =
      nanommo::Window::Create("nanoMMO bench", 1280, 720);
  if (!window.ok()) {
    std::cerr << "nanommo client bench: " << window.status() << "\n";
    return 1;
  }
  SetTargetFPS(0);
  const nanommo::Terrain terrain(seed);
  nanommo::Renderer renderer;
  renderer.set_terrain(terrain, nanommo::kMeadows);
  nanommo::FollowCamera camera;

  constexpr float kRadius = 80.0f;
  // The first frames load and warm up the GPU, so they don't count.
  constexpr int kWarmUp = 60;
  const float step = 2.0f * std::numbers::pi_v<float> / frames;
  std::vector<double> times;
  int64_t draws = 0;
  int64_t triangles = 0;
  double last = GetTime();
  for (int frame = -kWarmUp; frame < frames && !(*window)->ShouldClose();
       ++frame) {
    const float angle = std::max(frame, 0) * step;
    const float x = kRadius * std::sin(angle);
    const float z = kRadius * std::cos(angle);
    // Turning with us, so that it looks over us at the middle of the island.
    camera.Update({x, terrain.Height(x, z), z}, frame > 0 ? step : 0.0f, 0.0f,
                  0.0f, terrain);
    renderer.set_camera(camera.camera());
    BeginDrawing();
    renderer.Draw();
    EndDrawing();
    const double now = GetTime();
    if (frame >= 0) {
      times.push_back((now - last) * 1000.0);
      draws += renderer.work().draws;
      triangles += renderer.work().triangles;
    }
    last = now;
  }

  std::vector<double> sorted = times;
  std::ranges::sort(sorted);
  const auto percentile = [&sorted](double share) {
    return sorted[static_cast<std::size_t>(share * (sorted.size() - 1))];
  };
  double total = 0.0;
  for (const double time : times) {
    total += time;
  }
  const double fps = 1000.0 * times.size() / total;
  std::cout << "frames: " << times.size() << "\n"
            << "frames per second: " << fps << "\n"
            << "frame time p50: " << percentile(0.5) << " ms\n"
            << "frame time p99: " << percentile(0.99) << " ms\n"
            << "draws a frame: " << draws / std::ssize(times) << "\n"
            << "triangles a frame: " << triangles / std::ssize(times) << "\n";
  // And as JSON, for scripts and charts to read.
  if (argc > 3) {
    std::ofstream json(argv[3]);
    json << "{\"frames\": " << times.size() << ", \"fps\": " << fps
         << ", \"p50_ms\": " << percentile(0.5)
         << ", \"p99_ms\": " << percentile(0.99)
         << ", \"draws\": " << draws / std::ssize(times)
         << ", \"triangles\": " << triangles / std::ssize(times) << "}\n";
    if (!json) {
      std::cerr << "nanommo client bench: couldn't write " << argv[3] << "\n";
      return 1;
    }
  }
  return 0;
}
