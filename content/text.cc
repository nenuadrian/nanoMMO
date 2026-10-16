#include "content/text.h"

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

absl::Status ParseText(std::string_view text,
                       google::protobuf::Message& message) {
  Errors errors;
  google::protobuf::TextFormat::Parser parser;
  parser.RecordErrorsTo(&errors);
  if (!parser.ParseFromString(text, &message)) {
    return absl::InvalidArgumentError(errors.text());
  }
  return absl::OkStatus();
}

absl::StatusOr<std::string> ReadFile(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    return absl::NotFoundError(absl::StrCat("couldn't read ", path));
  }
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}

}  // namespace nanommo
