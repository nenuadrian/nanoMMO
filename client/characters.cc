#include "client/characters.h"

#include <cmath>
#include <cstring>
#include <string>

namespace nanommo {

namespace {

constexpr std::array<const char*, 2> kFiles = {"x_bot.glb", "y_bot.glb"};

constexpr std::array<const char*, 3> kClips = {"idle", "walk", "run"};

// raylib samples glTF clips sixty times a second, and the last frame is the
// first one again, so a clip loops after one frame fewer.
float FrameAt(const ModelAnimation& clip, float time) {
  const int frames = clip.keyframeCount > 1 ? clip.keyframeCount - 1 : 1;
  return std::fmod(time * 60.0f, static_cast<float>(frames));
}

}  // namespace

std::string CharactersFolder() {
  return std::string(GetApplicationDirectory()) + "characters";
}

Characters::Characters(const std::string& folder, Shader shader) {
  for (const char* file : kFiles) {
    const std::string path = folder + "/" + file;
    if (!FileExists(path.c_str())) {
      continue;
    }
    Character character{.model = LoadModel(path.c_str())};
    character.clips = LoadModelAnimations(path.c_str(), &character.clip_count);

    bool complete = true;
    for (std::size_t motion = 0; motion < kClips.size(); ++motion) {
      int found = -1;
      for (int i = 0; i < character.clip_count; ++i) {
        if (std::strcmp(character.clips[i].name, kClips[motion]) == 0 &&
            IsModelAnimationValid(character.model, character.clips[i])) {
          found = i;
        }
      }
      complete = complete && found >= 0;
      character.clip_for[motion] = found;
    }
    if (!complete) {
      TraceLog(LOG_WARNING, "%s doesn't have the clips we need", file);
      UnloadModelAnimations(character.clips, character.clip_count);
      UnloadModel(character.model);
      continue;
    }

    for (int i = 0; i < character.model.materialCount; ++i) {
      character.model.materials[i].shader = shader;
    }
    characters_.push_back(character);
  }
}

Characters::~Characters() {
  for (Character& character : characters_) {
    UnloadModelAnimations(character.clips, character.clip_count);
    UnloadModel(character.model);
  }
}

void Characters::Draw(const Figure& figure, Color color) const {
  const Character& character = characters_[figure.id % characters_.size()];
  const auto clip = [&](protocol::Motion motion) -> const ModelAnimation& {
    const int index = motion >= 0 && motion < 3 ? motion : 0;
    return character.clips[character.clip_for[index]];
  };
  const ModelAnimation& from = clip(figure.from);
  const ModelAnimation& to = clip(figure.to);

  // Players share the model's bones, so we pose them right before each draw.
  UpdateModelAnimationEx(character.model, from, FrameAt(from, figure.from_time),
                         to, FrameAt(to, figure.to_time), figure.blend);
  DrawModelEx(character.model, figure.position, {0.0f, 1.0f, 0.0f},
              figure.yaw * RAD2DEG, {1.0f, 1.0f, 1.0f}, color);
}

}  // namespace nanommo
