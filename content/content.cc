#include "content/content.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "absl/strings/str_cat.h"
#include "content/text.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

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

// FNV-1a, 64 bits wide, which gives the same hash on each machine, where
// std::hash and absl::Hash don't promise to.
constexpr uint64_t kOffsetBasis = 14695981039346656037u;
constexpr uint64_t kPrime = 1099511628211u;

void Add(uint64_t& hash, const google::protobuf::Message& message) {
  std::string bytes;
  {
    google::protobuf::io::StringOutputStream stream(&bytes);
    google::protobuf::io::CodedOutputStream coded(&stream);
    // So that equal messages give equal bytes, whatever order they were
    // built in.
    coded.SetSerializationDeterministic(true);
    message.SerializeToCodedStream(&coded);
  }
  for (const char byte : bytes) {
    hash ^= static_cast<unsigned char>(byte);
    hash *= kPrime;
  }
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

uint64_t ClientHash(const Content& content) {
  uint64_t hash = kOffsetBasis;
  // A copy of each with what the clients read, by id.
  for (const auto& [id, item] : content.items.by_id) {
    items::Item seen;
    seen.set_id(id);
    seen.set_name(item.name());
    *seen.mutable_look() = item.look();
    Add(hash, seen);
  }
  for (const auto& [id, kind] : content.monsters.kinds) {
    monsters::Kind seen;
    seen.set_id(id);
    seen.set_name(kind.name());
    *seen.mutable_look() = kind.look();
    Add(hash, seen);
  }
  for (const auto& [id, kind] : content.npcs.kinds) {
    npcs::Kind seen;
    seen.set_id(id);
    seen.set_name(kind.name());
    seen.set_talks(kind.talks());
    *seen.mutable_look() = kind.look();
    Add(hash, seen);
  }
  return hash;
}

}  // namespace nanommo
