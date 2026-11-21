#ifndef NANOMMO_CONTENT_RECIPES_H_
#define NANOMMO_CONTENT_RECIPES_H_

#include <cstdint>
#include <map>
#include <string_view>

#include "absl/status/statusor.h"
#include "content/recipes.pb.h"

namespace nanommo {

/// The recipes in text format, by id, or what's wrong with them: a field
/// recipes.proto doesn't have, a recipe with no id or with another's, one with
/// no level, one that takes no time, or nothing, or none of an item, or makes
/// none.
absl::StatusOr<std::map<uint32_t, recipes::Recipe>> ParseRecipes(
    std::string_view text);

}  // namespace nanommo

#endif  // NANOMMO_CONTENT_RECIPES_H_
