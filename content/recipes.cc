#include "content/recipes.h"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "content/checks.h"
#include "content/text.h"

namespace nanommo {

absl::StatusOr<std::map<uint32_t, recipes::Recipe>> ParseRecipes(
    std::string_view text) {
  recipes::Recipes file;
  const absl::Status parsed = ParseText(text, file);
  if (!parsed.ok()) {
    return parsed;
  }
  absl::StatusOr<std::map<uint32_t, recipes::Recipe>> by_id =
      ById(file.recipes());
  if (!by_id.ok()) {
    return by_id.status();
  }
  // Zero is what protobuf gives a field we leave out. A recipe whose level we
  // forgot would be everyone's, and one that takes nothing makes something
  // from nothing.
  for (const auto& [id, recipe] : *by_id) {
    const std::string& name = recipe.name();
    if (recipe.level() == 0) {
      return absl::InvalidArgumentError(absl::StrCat(name, " has no level"));
    }
    if (recipe.seconds() == 0) {
      return absl::InvalidArgumentError(absl::StrCat(name, " takes no time"));
    }
    if (recipe.inputs().empty()) {
      return absl::InvalidArgumentError(absl::StrCat(name, " takes nothing"));
    }
    for (const recipes::ItemCount& input : recipe.inputs()) {
      if (input.count() == 0) {
        return absl::InvalidArgumentError(
            absl::StrCat(name, " takes none of item ", input.item()));
      }
    }
    if (recipe.output().count() == 0) {
      return absl::InvalidArgumentError(
          absl::StrCat(name, " makes none of item ", recipe.output().item()));
    }
  }
  return by_id;
}

}  // namespace nanommo
