#ifndef NANOMMO_CONTENT_CONTENT_H_
#define NANOMMO_CONTENT_CONTENT_H_

#include <string>

#include "absl/status/statusor.h"
#include "content/items.h"

namespace nanommo {

/// What the game is made of, which the world servers and the clients both
/// read from the content folder.
struct Content {
  ItemTable items;
};

/// Reads each file in the folder, or says which file is wrong, and where.
absl::StatusOr<Content> ReadContent(const std::string& folder);

}  // namespace nanommo

#endif  // NANOMMO_CONTENT_CONTENT_H_
