// Films a film's shots through the game's own renderer, a frame at a time
// rather than as fast as the screen goes, so that the game's clock can run
// slower than the film's and each frame is drawn as long as it takes. Each
// shot goes into a video with the game's sounds, heard from the camera.
//
//   nanommo_cinema <film.txtpb> <out dir> [shots=a,b] [size=1920x1080]
//                  [fps=30] [still=seconds] [cel=0|1]
#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "cinema/film.h"
#include "client/animator.h"
#include "client/atmosphere.h"
#include "client/characters.h"
#include "client/foley.h"
#include "client/gate.h"
#include "client/graphics.h"
#include "client/postprocess.h"
#include "client/renderer.h"
#include "client/soundscape.h"
#include "client/synth.h"
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

// A sound the game would play, at a moment of the film, from somewhere.
struct Sound {
  nanommo::Samples samples;
  float at = 0.0f;
  /// How fast the game ran then, which slows the sound down too.
  float speed = 1.0f;
  float level = 1.0f;
  nanommo::Heard heard;
};

// A sound the game is going to play once its clock gets there: one of a
// skill's fx, at its own moment, so that a slowed skill's sounds come with
// what they go with.
struct Pending {
  float game = 0.0f;
  nanommo::Samples samples;
  Vector3 at;
};

// Slowed sounds drop in pitch, as on a tape played slower, but by less than
// the picture slows, or a quarter speed would take them two octaves down.
float Rate(float speed) { return std::clamp(std::sqrt(speed), 0.4f, 1.0f); }

void WriteWave(const std::string& path, const std::vector<float>& left,
               const std::vector<float>& right) {
  std::ofstream file(path, std::ios::binary);
  const auto u32 = [&](uint32_t v) {
    file.write(reinterpret_cast<char*>(&v), 4);
  };
  const auto u16 = [&](uint16_t v) {
    file.write(reinterpret_cast<char*>(&v), 2);
  };
  const uint32_t bytes = static_cast<uint32_t>(left.size() * 4);
  file.write("RIFF", 4);
  u32(36 + bytes);
  file.write("WAVEfmt ", 8);
  u32(16), u16(1), u16(2), u32(nanommo::kRate), u32(nanommo::kRate * 4);
  u16(4), u16(16);
  file.write("data", 4);
  u32(bytes);
  for (size_t i = 0; i < left.size(); ++i) {
    for (const float s : {left[i], right[i]}) {
      u16(static_cast<uint16_t>(
          static_cast<int16_t>(std::clamp(s, -1.0f, 1.0f) * 32767.0f)));
    }
  }
}

// The shot's sounds, mixed into two channels as long as the shot.
void Mix(const std::vector<Sound>& sounds, float seconds,
         const std::string& path) {
  const size_t length = static_cast<size_t>(seconds * nanommo::kRate);
  std::vector<float> left(length, 0.0f), right(length, 0.0f);
  for (const Sound& sound : sounds) {
    const float rate = Rate(sound.speed);
    const float gain = sound.level * sound.heard.volume;
    const float to_left = gain * std::min(1.0f, 1.0f - sound.heard.pan);
    const float to_right = gain * std::min(1.0f, 1.0f + sound.heard.pan);
    const size_t start = static_cast<size_t>(sound.at * nanommo::kRate);
    for (size_t i = 0; start + i < length; ++i) {
      const float from = static_cast<float>(i) * rate;
      const size_t a = static_cast<size_t>(from);
      if (a + 1 >= sound.samples.size()) {
        break;
      }
      const float s = Lerp(sound.samples[a], sound.samples[a + 1],
                           from - static_cast<float>(a));
      left[start + i] += s * to_left;
      right[start + i] += s * to_right;
    }
  }
  // Sounds at once can add up past one: rounded off, as a skill's own are.
  nanommo::Limit(left);
  nanommo::Limit(right);
  WriteWave(path, left, right);
}

int Shoot(const Shot& shot, const nanommo::Content& content,
          const Options& options) {
  const auto started = std::chrono::steady_clock::now();
  const int land = nanommo::cinema::LandOf(shot);
  const nanommo::Terrain terrain = nanommo::cinema::TerrainOf(land);
  nanommo::Renderer renderer(content);
  renderer.set_cel_shading(options.cel);
  renderer.set_terrain(terrain, nanommo::kLooks[land]);
  renderer.set_cubes(nanommo::GateCubes(terrain, land));
  nanommo::Animator animator(content);
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
        name, ".video.mp4");
    video = popen(encode.c_str(), "w");
    if (video == nullptr) {
      std::cerr << "nanommo_cinema: couldn't start ffmpeg\n";
      return 1;
    }
  }

  nanommo::cinema::Clock clock(shot);
  nanommo::cinema::Stage stage(shot, content);
  nanommo::Footsteps footsteps;
  nanommo::Casts casts;
  std::vector<Sound> sounds;
  std::vector<Pending> pending;
  uint32_t seed = 1;
  const int frames =
      static_cast<int>(std::lround(shot.seconds() * options.fps));
  const int still =
      options.still < 0.0f
          ? -1
          : static_cast<int>(std::lround(options.still * options.fps));
  float moved = 0.0f;
  for (int frame = 0; frame < frames && (still < 0 || frame <= still);
       ++frame) {
    const float now = clock.film();
    const float game = clock.game();
    std::vector<nanommo::Figure> figures =
        animator.Update(stage.At(game), moved);
    for (nanommo::Figure& figure : figures) {
      figure.position.y = terrain.Height(figure.position.x, figure.position.z);
    }
    renderer.set_figures(std::move(figures));
    renderer.Animate(moved);
    const Camera3D camera = nanommo::cinema::CameraAt(shot, now, terrain);
    renderer.set_camera(camera);

    // What the game would play now, heard where the camera looks, with the
    // camera's right to the right.
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(
        Vector3Subtract(camera.target, camera.position), camera.up));
    const float speed = nanommo::cinema::SpeedAt(shot, now);
    std::map<uint64_t, nanommo::Gait> gaits;
    std::map<uint64_t, Vector3> where;
    for (const nanommo::Figure& figure : renderer.figures()) {
      if (const auto gait = renderer.GaitOf(figure)) {
        gaits[nanommo::Who(figure)] = *gait;
        where[nanommo::Who(figure)] = figure.position;
      }
    }
    for (const uint64_t who : footsteps.Update(gaits)) {
      const Vector3 at = where.at(who);
      sounds.push_back(
          {.samples = nanommo::Step(
               nanommo::GroundAt(terrain, land, at.x, at.z), seed++),
           .at = now,
           .speed = speed,
           .level = 0.5f,
           .heard = nanommo::Hear(at, camera.target, right)});
    }
    for (const nanommo::Figure& figure : casts.Update(renderer.figures())) {
      const auto skill = content.skills.by_id.find(figure.to_skill);
      if (skill == content.skills.by_id.end()) {
        continue;
      }
      // Each of its fx's sounds at the fx's own moment of the game, as the
      // fx is drawn then, however slowly the game runs by then.
      for (const nanommo::skills::Fx& fx : skill->second.look().fx()) {
        if (fx.at() == nanommo::skills::Fx::TARGET && figure.target == 0) {
          continue;
        }
        pending.push_back({.game = game + fx.from_ms() / 1000.0f,
                           .samples = nanommo::FxSound(fx, seed++),
                           .at = figure.position});
      }
    }
    // Those whose moment has come.
    for (auto it = pending.begin(); it != pending.end();) {
      if (it->game > game) {
        ++it;
        continue;
      }
      sounds.push_back({.samples = std::move(it->samples),
                        .at = now,
                        .speed = speed,
                        .level = 1.0f,
                        .heard = nanommo::Hear(it->at, camera.target, right)});
      it = pending.erase(it);
    }

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
    moved = clock.Advance(1.0f / options.fps);
  }
  UnloadRenderTexture(picture);
  if (video != nullptr) {
    pclose(video);
    Mix(sounds, shot.seconds(), name + ".wav");
    const std::string mux =
        absl::StrCat("ffmpeg -loglevel error -y -i ", name, ".video.mp4 -i ",
                     name, ".wav -c:v copy -c:a aac -b:a 256k -shortest ", name,
                     ".mp4 && rm ", name, ".video.mp4");
    if (std::system(mux.c_str()) != 0) {
      std::cerr << "nanommo_cinema: couldn't add the sound to " << name << '\n';
      return 1;
    }
  }
  const float took =
      std::chrono::duration<float>(std::chrono::steady_clock::now() - started)
          .count();
  std::cout << shot.id() << ": " << frames << " frames, " << shot.seconds()
            << " s of film, " << clock.game() - shot.from()
            << " s of game, drawn in " << took << " s\n";
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
