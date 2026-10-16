#ifndef NANOMMO_CONTENT_TEXT_H_
#define NANOMMO_CONTENT_TEXT_H_

#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "google/protobuf/message.h"

namespace nanommo {

/// Parses protobuf's text format into the message, or says on which line it
/// went wrong, such as at a field the message doesn't have.
absl::Status ParseText(std::string_view text,
                       google::protobuf::Message& message);

absl::StatusOr<std::string> ReadFile(const std::string& path);

}  // namespace nanommo

#endif  // NANOMMO_CONTENT_TEXT_H_
