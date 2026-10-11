#include "client/characters.h"

#include <external/cgltf.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace nanommo {

namespace {

constexpr std::array<const char*, 3> kClips = {"idle", "walk", "run"};

constexpr std::string_view kRightHand = "mixamorig:RightHand";

// The middle of the sword's grip is a sixth of the way up from the pommel.
constexpr float kGrip = 1.0f / 6.0f;

constexpr float kPalm = 0.08f;

// raylib cuts clip names at 31 characters, so the skeleton's run ends in
// Runni, which still has the word run in it.
bool HasWord(std::string name, std::string_view word) {
  std::ranges::transform(name, name.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return name.find(word) != std::string::npos;
}

// raylib samples glTF clips sixty times a second, and the last frame is the
// first one again, so a clip loops after one frame fewer.
float FrameAt(const ModelAnimation& clip, float time) {
  const int frames = clip.keyframeCount > 1 ? clip.keyframeCount - 1 : 1;
  return std::fmod(time * 60.0f, static_cast<float>(frames));
}

// glTF stores a matrix a column at a time, and raylib's Matrix lists its
// fields a row at a time.
Matrix FromGltf(const std::array<float, 16>& m) {
  return {m[0], m[4], m[8],  m[12], m[1], m[5], m[9],  m[13],
          m[2], m[6], m[10], m[14], m[3], m[7], m[11], m[15]};
}

// raylib takes the pose the bones rest in as the one the mesh was bound in,
// but the skeleton's arms rest about 70 degrees away from it. glTF keeps the
// bind pose as each bone's inverse bind matrix, so we read it from those.
void ReadBindPose(const std::string& path, Model& model) {
  cgltf_options options{};
  cgltf_data* data = nullptr;
  if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success) {
    return;
  }
  const cgltf_node* mesh = nullptr;
  for (cgltf_size i = 0; i < data->nodes_count && mesh == nullptr; ++i) {
    if (data->nodes[i].skin != nullptr) {
      mesh = &data->nodes[i];
    }
  }
  if (mesh != nullptr && mesh->skin->inverse_bind_matrices != nullptr &&
      cgltf_load_buffers(&options, data, path.c_str()) ==
          cgltf_result_success) {
    std::array<float, 16> world{};
    cgltf_node_transform_world(mesh, world.data());
    // raylib moves the mesh to where its node puts it in the world as it
    // loads it, so the bind pose goes there too.
    for (int i = 0; i < model.skeleton.boneCount; ++i) {
      std::array<float, 16> inverse{};
      if (cgltf_accessor_read_float(mesh->skin->inverse_bind_matrices, i,
                                    inverse.data(), inverse.size())) {
        const Matrix bind =
            MatrixMultiply(MatrixInvert(FromGltf(inverse)), FromGltf(world));
        Transform& pose = model.skeleton.bindPose[i];
        MatrixDecompose(bind, &pose.translation, &pose.rotation, &pose.scale);
      }
    }
  }
  cgltf_free(data);
}

}  // namespace

std::string CharactersFolder() {
  return std::string(GetApplicationDirectory()) + "characters";
}

std::string MonstersFolder() {
  return std::string(GetApplicationDirectory()) + "monsters";
}

std::string WeaponsFolder() {
  return std::string(GetApplicationDirectory()) + "weapons";
}

Characters::Characters(const std::string& folder,
                       const std::vector<std::string>& files, float height,
                       Shader shader) {
  for (const std::string& file : files) {
    const std::string path = folder + "/" + file;
    if (!FileExists(path.c_str())) {
      continue;
    }
    Character character{.model = LoadModel(path.c_str())};
    ReadBindPose(path, character.model);
    character.clips = LoadModelAnimations(path.c_str(), &character.clip_count);

    for (std::size_t motion = 0; motion < kClips.size(); ++motion) {
      int found = -1;
      for (int i = 0; i < character.clip_count; ++i) {
        if (HasWord(character.clips[i].name, kClips[motion]) &&
            IsModelAnimationValid(character.model, character.clips[i])) {
          found = i;
        }
      }
      character.clip_for[motion] = found;
    }
    // Without a walk, a character walks with its run, played slower. The
    // skeleton's feet keep up with our walk at 0.4.
    auto& [idle, walk, run] = character.clip_for;
    if (walk < 0 && run >= 0) {
      walk = run;
      character.speed[protocol::MOTION_WALK] = 0.4f;
    }
    if (idle < 0 || walk < 0 || run < 0) {
      TraceLog(LOG_WARNING, "%s doesn't have the clips we need", file.c_str());
      UnloadModelAnimations(character.clips, character.clip_count);
      UnloadModel(character.model);
      continue;
    }

    for (int i = 0; i < character.model.materialCount; ++i) {
      character.model.materials[i].shader = shader;
    }
    for (int i = 0; i < character.model.skeleton.boneCount; ++i) {
      if (character.model.skeleton.bones[i].name == kRightHand) {
        character.hand = i;
      }
    }
    const BoundingBox box = GetModelBoundingBox(character.model);
    character.scale = height / (box.max.y - box.min.y);
    characters_.push_back(character);
  }
}

Characters::~Characters() {
  for (Character& character : characters_) {
    UnloadModelAnimations(character.clips, character.clip_count);
    UnloadModel(character.model);
  }
  if (weapon_) {
    UnloadModel(*weapon_);
  }
}

void Characters::Hold(const std::string& path, float length, Shader shader) {
  if (!FileExists(path.c_str())) {
    return;
  }
  Model weapon = LoadModel(path.c_str());
  // glTF colours are linear, and our shader writes them out as they are, so
  // we turn them into sRGB, or the sword comes out nearly black.
  for (int i = 0; i < weapon.materialCount; ++i) {
    weapon.materials[i].shader = shader;
    Color& color = weapon.materials[i].maps[MATERIAL_MAP_DIFFUSE].color;
    const Vector4 linear = ColorNormalize(color);
    color = ColorFromNormalized({std::pow(linear.x, 1.0f / 2.2f),
                                 std::pow(linear.y, 1.0f / 2.2f),
                                 std::pow(linear.z, 1.0f / 2.2f), linear.w});
  }
  const BoundingBox box = GetModelBoundingBox(weapon);
  const float height = box.max.y - box.min.y;
  const float scale = length / height;
  const Matrix sized =
      MatrixMultiply(MatrixTranslate(0.0f, -(box.min.y + kGrip * height), 0.0f),
                     MatrixScale(scale, scale, scale));
  // Mixamo's hand: y along the fingers, x along the knuckles, palm facing z.
  // The fist holds the blade along x, leaning 20 degrees towards the fingers.
  const Matrix along = MatrixRotateZ(-(90.0f - 20.0f) * DEG2RAD);
  // Mixamo's clips turn the hands in, so we turn the blade 40 degrees out,
  // clear of the legs.
  const Matrix out = MatrixRotateY(40.0f * DEG2RAD);
  const Matrix in_hand = MatrixMultiply(MatrixMultiply(along, out),
                                        MatrixTranslate(0.0f, kPalm, 0.0f));
  grip_ = MatrixMultiply(sized, in_hand);
  weapon_ = weapon;
}

void Characters::Draw(const Figure& figure, Color color) const {
  const Character& character = characters_[figure.id % characters_.size()];
  const auto index = [](protocol::Motion motion) {
    return motion >= 0 && motion < 3 ? motion : 0;
  };
  const int from = index(figure.from);
  const int to = index(figure.to);
  const ModelAnimation& from_clip = character.clips[character.clip_for[from]];
  const ModelAnimation& to_clip = character.clips[character.clip_for[to]];

  // Players share the model's bones, so we pose them right before each draw.
  UpdateModelAnimationEx(
      character.model, from_clip,
      FrameAt(from_clip, figure.from_time * character.speed[from]), to_clip,
      FrameAt(to_clip, figure.to_time * character.speed[to]), figure.blend);
  const float scale = character.scale;
  DrawModelEx(character.model, figure.position, {0.0f, 1.0f, 0.0f},
              figure.yaw * RAD2DEG, {scale, scale, scale}, color);

  if (!weapon_ || character.hand < 0) {
    return;
  }
  const Transform& hand = character.model.currentPose[character.hand];
  const Matrix bone = MatrixMultiply(
      MatrixMultiply(MatrixScale(hand.scale.x, hand.scale.y, hand.scale.z),
                     QuaternionToMatrix(hand.rotation)),
      MatrixTranslate(hand.translation.x, hand.translation.y,
                      hand.translation.z));
  const Matrix placed = MatrixMultiply(
      MatrixMultiply(MatrixScale(scale, scale, scale),
                     MatrixRotateY(figure.yaw)),
      MatrixTranslate(figure.position.x, figure.position.y, figure.position.z));
  Model weapon = *weapon_;
  weapon.transform = MatrixMultiply(MatrixMultiply(grip_, bone), placed);
  DrawModel(weapon, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
}

}  // namespace nanommo
