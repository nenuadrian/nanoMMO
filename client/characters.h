#ifndef NANOMMO_CLIENT_CHARACTERS_H_
#define NANOMMO_CLIENT_CHARACTERS_H_

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "client/animator.h"
#include "client/outline.h"
#include "content/figures.pb.h"
#include "content/items.pb.h"
#include "content/skills.pb.h"

namespace nanommo {

std::string CharactersFolder();

std::string MonstersFolder();

std::string WeaponsFolder();

std::string ContentFolder();

class Characters {
 public:
  /// With the figures, each model is dressed in their body in place of its
  /// mesh, and each player is drawn in their own skin and hair.
  Characters(const std::string& folder, const std::vector<std::string>& files,
             float height, Shader shader,
             const figures::Figures& figures = {});
  ~Characters();

  Characters(const Characters&) = delete;
  Characters& operator=(const Characters&) = delete;

  std::size_t size() const { return characters_.size(); }

  /// Loads the item's model, to draw in the hand of each figure that holds
  /// the item.
  void Hold(uint32_t id, const items::Look& look, Shader shader);

  /// Finds the skill's clip, by its name, in each character that has it.
  void Learn(uint32_t skill, const skills::Look& look);

  /// Draws the figure, posed, with its weapon, and with an outline, when it's
  /// given one: a model in the colour, and a player's figure in their skin,
  /// with their hair.
  void Draw(const Figure& figure, Color color, const Outline* outline) const;

 private:
  /// Something over the figure, as a model that shares the character's bones.
  struct Garment {
    Model model;
  };

  struct Character {
    Model model;
    bool dressed = false;
    /// Each way to wear the hair, by its place in the figures' list, but
    /// bald.
    std::map<int, Garment> hairs;
    ModelAnimation* clips = nullptr;
    int clip_count = 0;
    std::array<int, 5> clip_for{};
    std::array<float, 5> speed{1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    float scale = 1.0f;
    int hand = -1;
    /// Each skill's clip, by the skill's id.
    std::map<uint32_t, int> skill_clips;
  };

  struct Pose {
    const ModelAnimation* clip;
    float frame;
  };

  static Pose PoseFor(const Character& character, protocol::Motion motion,
                      uint32_t skill, float time);

  struct Weapon {
    Model model;
    Matrix grip;
  };

  figures::Figures figures_;
  std::vector<Character> characters_;
  std::map<uint32_t, Weapon> weapons_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_CHARACTERS_H_
