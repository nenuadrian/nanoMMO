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
#include "items/items.pb.h"

namespace nanommo {

std::string CharactersFolder();

std::string MonstersFolder();

std::string WeaponsFolder();

std::string ItemsFile();

class Characters {
 public:
  Characters(const std::string& folder, const std::vector<std::string>& files,
             float height, Shader shader);
  ~Characters();

  Characters(const Characters&) = delete;
  Characters& operator=(const Characters&) = delete;

  std::size_t size() const { return characters_.size(); }

  /// Loads the item's model, to draw in the hand of each figure that holds
  /// the item.
  void Hold(uint32_t id, const items::Look& look, Shader shader);

  void Draw(const Figure& figure, Color color) const;

 private:
  struct Character {
    Model model;
    ModelAnimation* clips = nullptr;
    int clip_count = 0;
    std::array<int, 5> clip_for{};
    std::array<float, 5> speed{1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    float scale = 1.0f;
    int hand = -1;
  };

  struct Weapon {
    Model model;
    Matrix grip;
  };

  std::vector<Character> characters_;
  std::map<uint32_t, Weapon> weapons_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_CHARACTERS_H_
