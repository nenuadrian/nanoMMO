#include "server/characters.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "server/accounts.h"

namespace nanommo {

absl::StatusOr<uint32_t> Characters::Create(uint32_t account,
                                            const std::string& world,
                                            const std::string& name) {
  if (const absl::Status checked = CheckUsername(name); !checked.ok()) {
    return checked;
  }
  // Counting and inserting in one statement, so that two at once can't both
  // fit in the last place. The unique key on the world and the name says
  // whether it's taken.
  const absl::StatusOr<Result> inserted = database_.Run(
      "INSERT INTO players (account_id, world, name, x, z) "
      "SELECT ?, ?, ?, 0, 0 FROM DUAL WHERE "
      "(SELECT COUNT(*) FROM players WHERE account_id = ? AND world = ?) < ?",
      {absl::StrCat(account), world, name, absl::StrCat(account), world,
       absl::StrCat(kPerWorld)});
  if (absl::IsAlreadyExists(inserted.status())) {
    return absl::AlreadyExistsError(
        absl::StrCat("Someone in ", world, " is called that already."));
  }
  if (!inserted.ok()) {
    return inserted.status();
  }
  if (inserted->insert_id == 0) {
    return absl::ResourceExhaustedError(absl::StrCat(
        "You have ", kPerWorld, " characters in ", world, " already."));
  }
  return static_cast<uint32_t>(inserted->insert_id);
}

}  // namespace nanommo
