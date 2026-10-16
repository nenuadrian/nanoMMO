#include "content/content.h"

#include <string>
#include <string_view>
#include <utility>

#include "absl/strings/str_cat.h"
#include "content/text.h"

namespace nanommo {

namespace {

// Reads one of the folder's files into its table, with the file's name in
// front of what's wrong with it.
template <typename Table>
absl::Status Read(const std::string& folder, const char* file,
                  absl::StatusOr<Table> (*parse)(std::string_view),
                  Table& table) {
  const std::string path = absl::StrCat(folder, "/", file);
  const absl::StatusOr<std::string> text = ReadFile(path);
  if (!text.ok()) {
    return text.status();
  }
  absl::StatusOr<Table> parsed = parse(*text);
  if (!parsed.ok()) {
    return absl::InvalidArgumentError(
        absl::StrCat(path, ", ", parsed.status().message()));
  }
  table = *std::move(parsed);
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<Content> ReadContent(const std::string& folder) {
  Content content;
  absl::Status read = Read(folder, "items.txtpb", ParseItems, content.items);
  if (read.ok()) {
    read = Read(folder, "monsters.txtpb", ParseMonsters, content.monsters);
  }
  if (read.ok()) {
    read = Read(folder, "npcs.txtpb", ParseNpcs, content.npcs);
  }
  if (!read.ok()) {
    return read;
  }
  return content;
}

}  // namespace nanommo
