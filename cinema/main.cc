// Films a film's shots through the game's own renderer, a frame at a time
// rather than as fast as the screen goes, so that each frame is drawn as long
// as it takes, at any size. Each shot goes into a video.
//
//   nanommo_cinema <film.txtpb> <out dir> [shots=a,b] [size=1920x1080]
//                  [fps=30] [still=seconds] [cel=0|1]
#include <raylib.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "cinema/film.h"
#include "client/atmosphere.h"
#include "client/characters.h"
#include "client/gate.h"
#include "client/graphics.h"
#include "client/postprocess.h"
#include "client/renderer.h"
#include "content/content.h"

namespace {

using nanommo::cinema::Shot;

struct Options {
  std::string out;
  int width = 1920;
  int height = 1080;
  float fps = 30.0f;
  bool cel = true;
  /// A single frame, at this second of each shot, as a picture: -1 for none.
  float still = -1.0f;
};

int Shoot(const Shot& shot, const nanommo::Content& content,
          const Options& options) {
  const auto started = std::chrono::steady_clock::now();
  const int land = nanommo::cinema::LandOf(shot);
  const nanommo::Terrain terrain = nanommo::cinema::TerrainOf(land);
  nanommo::Renderer renderer(content);
  renderer.set_cel_shading(options.cel);
  renderer.set_terrain(terrain, nanommo::kLooks[land]);
  renderer.set_cubes(nanommo::GateCubes(terrain, land));
  nanommo::PostProcess post;
  const nanommo::GraphicsSettings graphics{.cel_shading = options.cel};
  const RenderTexture2D picture =
      LoadRenderTexture(options.width, options.height);

  const std::string name = absl::StrCat(options.out, "/", shot.id());
  FILE* video = nullptr;
  if (options.still < 0.0f) {
    const std::string encode = absl::StrCat(
        "ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgba -s ",
        options.width, "x", options.height, " -r ", options.fps,
        " -i - -vf vflip -c:v libx264 -preset fast -crf 14 -pix_fmt yuv420p ",
        name, ".mp4");
    video = popen(encode.c_str(), "w");
    if (video == nullptr) {
      std::cerr << "nanommo_cinema: couldn't start ffmpeg\n";
      return 1;
    }
  }

  const int frames =
      static_cast<int>(std::lround(shot.seconds() * options.fps));
  const int still =
      options.still < 0.0f
          ? -1
          : static_cast<int>(std::lround(options.still * options.fps));
  for (int frame = 0; frame < frames && (still < 0 || frame <= still);
       ++frame) {
    const float now = frame / options.fps;
    // The sea's waves and anything else that moves by itself, a frame on.
    renderer.Animate(1.0f / options.fps);
    const Camera3D camera = nanommo::cinema::CameraAt(shot, now, terrain);
    renderer.set_camera(camera);

    if (still < 0 || frame == still) {
      post.Begin(graphics, options.width, options.height);
      renderer.Draw();
      post.End();
      post.DrawOnto(picture);
      Image image = LoadImageFromTexture(picture.texture);
      if (video != nullptr) {
        fwrite(image.data, 4,
               static_cast<size_t>(options.width) * options.height, video);
      } else {
        ImageFlipVertical(&image);
        ExportImage(image, absl::StrCat(name, ".png").c_str());
      }
      UnloadImage(image);
    }
  }
  UnloadRenderTexture(picture);
  if (video != nullptr) {
    pclose(video);
  }
  const float took =
      std::chrono::duration<float>(std::chrono::steady_clock::now() - started)
          .count();
  std::cout << shot.id() << ": " << frames << " frames, " << shot.seconds()
            << " s, drawn in " << took << " s\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr
        << "usage: nanommo_cinema <film.txtpb> <out dir> [key=value ...]\n";
    return 2;
  }
  Options options;
  options.out = argv[2];
  std::set<std::string> wanted;
  for (int i = 3; i < argc; ++i) {
    const std::vector<std::string> pair = absl::StrSplit(argv[i], '=');
    if (pair.size() != 2) {
      std::cerr << "nanommo_cinema: not key=value: " << argv[i] << '\n';
      return 2;
    }
    const std::string& key = pair[0];
    const std::string& value = pair[1];
    if (key == "shots") {
      for (const auto& id : absl::StrSplit(value, ',')) {
        wanted.emplace(id);
      }
    } else if (key == "size") {
      std::sscanf(value.c_str(), "%dx%d", &options.width, &options.height);
    } else if (key == "fps") {
      options.fps = std::strtof(value.c_str(), nullptr);
    } else if (key == "cel") {
      options.cel = value == "1";
    } else if (key == "still") {
      options.still = std::strtof(value.c_str(), nullptr);
    } else {
      std::cerr << "nanommo_cinema: unknown key " << key << '\n';
      return 2;
    }
  }
  const absl::StatusOr<nanommo::cinema::Film> film =
      nanommo::cinema::ReadFilm(argv[1]);
  if (!film.ok()) {
    std::cerr << "nanommo_cinema: " << film.status().message() << '\n';
    return 1;
  }
  // The window only gives us a GL context: the frames are drawn into render
  // textures of the film's size, whatever the screen's.
  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(320, 180, "nanoMMO cinema");
  if (!IsWindowReady()) {
    return 1;
  }
  SetTargetFPS(0);
  const auto content = nanommo::ReadContent(nanommo::ContentFolder());
  if (!content.ok()) {
    std::cerr << "nanommo_cinema: " << content.status().message() << '\n';
    return 1;
  }
  int failed = 0;
  for (const Shot& shot : film->shots()) {
    if (wanted.empty() || wanted.contains(shot.id())) {
      failed += Shoot(shot, *content, options);
    }
  }
  CloseWindow();
  return failed == 0 ? 0 : 1;
}
