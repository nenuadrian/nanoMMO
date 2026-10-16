#include "items/items.h"

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "google/protobuf/io/tokenizer.h"
#include "google/protobuf/text_format.h"

namespace nanommo {

namespace {

class Errors : public google::protobuf::io::ErrorCollector {
 public:
  void RecordError(int line, google::protobuf::io::ColumnNumber /*column*/,
                   absl::string_view message) override {
    // The parser counts lines from zero, and editors count them from one.
    absl::StrAppend(&text_, "line ", line + 1, ": ", message);
  }

  const std::string& text() const { return text_; }

 private:
  std::string text_;
};

}  // namespace

absl::StatusOr<ItemTable> ParseItems(std::string_view text) {
  items::Items file;
  Errors errors;
  google::protobuf::TextFormat::Parser parser;
  parser.RecordErrorsTo(&errors);
  if (!parser.ParseFromString(text, &file)) {
    return absl::InvalidArgumentError(errors.text());
  }
  ItemTable table{.starting_weapon = file.starting_weapon()};
  for (const items::Item& item : file.items()) {
    if (item.id() == 0) {
      return absl::InvalidArgumentError(
          absl::StrCat("the ", item.name(), " has no id"));
    }
    if (!table.by_id.emplace(item.id(), item).second) {
      return absl::InvalidArgumentError(
          absl::StrCat("two items have the id ", item.id()));
    }
  }
  const auto starting = table.by_id.find(table.starting_weapon);
  if (starting == table.by_id.end() || !starting->second.has_weapon()) {
    return absl::InvalidArgumentError("the starting weapon isn't a weapon");
  }
  return table;
}

absl::StatusOr<ItemTable> ReadItems(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    return absl::NotFoundError(absl::StrCat("couldn't read ", path));
  }
  std::ostringstream text;
  text << file.rdbuf();
  absl::StatusOr<ItemTable> table = ParseItems(text.str());
  if (!table.ok()) {
    return absl::InvalidArgumentError(
        absl::StrCat(path, ", ", table.status().message()));
  }
  return table;
}

}  // namespace nanommo
