#include "client/predicted_bag.h"

#include "absl/status/status.h"
#include "server/inventory.h"

namespace nanommo {

protocol::Inventory PredictedBag::Shown(const protocol::Inventory& server,
                                        float seconds) {
  if (!ahead_) {
    return server;
  }
  waited_ += seconds;
  if (server.revision() != revision_ || waited_ > kPatience) {
    ahead_.reset();
    return server;
  }
  return *ahead_;
}

void PredictedBag::Act(const protocol::Inventory& server,
                       const protocol::ItemAction& action) {
  // On top of what's shown, so that two quick moves both show.
  Inventory bag(ahead_ ? *ahead_ : server);
  absl::Status done = absl::OkStatus();
  switch (action.action_case()) {
    case protocol::ItemAction::kMove:
      done = bag.Move(items_, action.move().from(), action.move().to(),
                      action.move().count());
      break;
    case protocol::ItemAction::kUse:
      done = bag.TakeOne(InBag(action.use()));
      break;
    case protocol::ItemAction::kDestroy:
      done = bag.Destroy(action.destroy());
      break;
    case protocol::ItemAction::ACTION_NOT_SET:
      return;
  }
  if (!done.ok()) {
    return;
  }
  if (!ahead_) {
    revision_ = server.revision();
  }
  ahead_ = bag.items();
  waited_ = 0.0f;
}

}  // namespace nanommo
