#ifndef NANOMMO_CLIENT_HELP_WINDOW_H_
#define NANOMMO_CLIENT_HELP_WINDOW_H_

namespace nanommo {

/// How to play, on H: a page for each part of the game, such as moving,
/// fighting, items, quests, guilds and chat, and the keys.
class HelpWindow {
 public:
  bool* mutable_visible() { return &visible_; }
  void Toggle() { visible_ = !visible_; }

  void Build();

 private:
  enum class Page {
    kMoving,
    kFighting,
    kItems,
    kQuests,
    kGuilds,
    kChat,
    kKeys
  };

  bool visible_ = false;
  Page page_ = Page::kMoving;
};

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_HELP_WINDOW_H_
