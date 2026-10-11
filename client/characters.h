#ifndef NANOMMO_CLIENT_CHARACTERS_H_
#define NANOMMO_CLIENT_CHARACTERS_H_

#include <raylib.h>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "client/animator.h"

namespace nanommo {

std::string CharactersFolder();

class Characters {
 public:
  Characters(const std::string& folder, Shader shader);
  ~Characters();

  Characters(const Characters&) = delete;
  Characters& operator=(const Characters&) = delete;

  std::size_t size() const { return characters_.size(); }

  void Draw(const Figure& figure, Color color) const;

 private:
  struct Character {
    Model model;
    ModelAnimation* clips = nullptr;
    int clip_count = 0;
    std::array<int, 3> clip_for{};
  };

  std::vector<Character> characters_;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_CHARACTERS_H_
