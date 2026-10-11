#ifndef NANOMMO_CLIENT_CHARACTERS_H_
#define NANOMMO_CLIENT_CHARACTERS_H_

#include <raylib.h>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "client/animator.h"

namespace nanommo {

std::string CharactersFolder();

std::string MonstersFolder();

std::string WeaponsFolder();

class Characters {
 public:
  Characters(const std::string& folder, const std::vector<std::string>& files,
             float height, Shader shader);
  ~Characters();

  Characters(const Characters&) = delete;
  Characters& operator=(const Characters&) = delete;

  std::size_t size() const { return characters_.size(); }

  void Hold(const std::string& path, float length, Shader shader);

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

  std::vector<Character> characters_;
  std::optional<Model> weapon_;
  Matrix grip_{};
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_CHARACTERS_H_
