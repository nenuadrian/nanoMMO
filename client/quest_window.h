#ifndef NANOMMO_CLIENT_QUEST_WINDOW_H_
#define NANOMMO_CLIENT_QUEST_WINDOW_H_

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "content/content.h"
#include "protocol/nanommo.pb.h"

namespace nanommo {

/// A window with the quests an NPC gives, which opens when the player clicks
/// them.
class QuestWindow {
 public:
  explicit QuestWindow(Content content) : content_(std::move(content)) {}

  void Open(uint32_t npc, std::string name, std::vector<uint32_t> quests);

  /// Draws the window, while it's open: what the NPC says giving each quest,
  /// what it asks for and what it gives, with a button to take it, how far
  /// the player is with it, or a button to turn it in once it's done. Returns
  /// what the player asks for, as an input with the quest to take or turn in.
  std::optional<protocol::Input> Build(const protocol::Quests& yours);

 private:
  Content content_;
  std::optional<uint32_t> npc_;
  std::string name_;
  std::vector<uint32_t> quests_;
};

/// The quests the player's on, in the top right corner, with how many
/// they've killed for each.
void DrawQuestTracker(const Content& content, const protocol::Quests& yours);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_QUEST_WINDOW_H_
