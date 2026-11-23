#include "client/messages.h"

#include <raylib.h>

#include <cstdint>
#include <fstream>
#include <string>

#include "client/translations.h"
#include "content/content.h"
#include "gtest/gtest.h"
#include "protocol/messages.h"

namespace {

// Romanian for a sentence from a server, and for the item in it.
constexpr char kPo[] = R"po(msgid ""
msgstr ""
"Language: ro\n"
"Plural-Forms: nplurals=3; plural=(n==1 ? 0 : (n==0 || (n%100 > 0 && n%100 < 20)) ? 1 : 2);\n"

msgid "Your bag is full. Make room for $0 first."
msgstr "Rucsacul e plin. E nevoie de loc pentru $0."

msgctxt "item 4"
msgid "Cap"
msgstr "Căciulă"

msgctxt "item 4"
msgid "a Cap"
msgid_plural "$0 Caps"
msgstr[0] "o căciulă"
msgstr[1] "$0 căciuli"
msgstr[2] "$0 de căciuli"

msgid "$0 asks you to join $1."
msgstr "Invitație de la $0 în $1."
)po";

class Messages : public testing::Test {
 protected:
  void SetUp() override {
    nanommo::items::Item cap;
    cap.set_id(4);
    cap.set_name("Cap");
    content_.items.by_id[4] = cap;
  }

  // The interface goes back to English after each test.
  void TearDown() override { ASSERT_TRUE(nanommo::UseLanguage("", "").ok()); }

  void InRomanian() {
    const std::string folder = testing::TempDir() + "messages";
    MakeDirectory(folder.c_str());
    std::ofstream(folder + "/ro.po") << kPo;
    ASSERT_TRUE(nanommo::UseLanguage("ro", folder).ok());
  }

  nanommo::Sentence Full(uint32_t caps) const {
    return nanommo::Msg("Your bag is full. Make room for $0 first.",
                        nanommo::Of(content_.items, 4, caps));
  }

  nanommo::Content content_;
};

TEST_F(Messages, AreTheServersEnglishInEnglish) {
  for (const uint32_t caps : {1u, 3u}) {
    const nanommo::Sentence full = Full(caps);
    EXPECT_EQ(nanommo::Translated(full.message, content_), full.english);
  }
}

TEST_F(Messages, AreTranslatedWithTheContentsNames) {
  InRomanian();
  EXPECT_EQ(nanommo::Translated(Full(1).message, content_),
            "Rucsacul e plin. E nevoie de loc pentru o căciulă.");
  EXPECT_EQ(nanommo::Translated(Full(20).message, content_),
            "Rucsacul e plin. E nevoie de loc pentru 20 de căciuli.");
  // The name alone, as the item's own.
  EXPECT_EQ(
      nanommo::Translated(
          nanommo::Msg("$0", nanommo::Of(content_.items.by_id[4])).message,
          content_),
      "Căciulă");
}

TEST_F(Messages, AreInTheLanguagesFileWithTheContentsNames) {
  // The servers' sentences, and so many of each item, are in the template
  // that scripts/po.py writes, and in Romanian.
  const nanommo::Content content = *nanommo::ReadContent(NANOMMO_CONTENT);
  ASSERT_TRUE(nanommo::UseLanguage("ro", NANOMMO_PO).ok());
  const nanommo::Sentence dropped = nanommo::Msg(
      "The $0 dropped $1.", nanommo::Of(content.monsters.kinds.at(1)),
      nanommo::Msg("$0 and $1", nanommo::Of(content.items, 3, 20),
                   nanommo::MsgN("$0 gold", "$0 gold", 5, 5)));
  EXPECT_EQ(nanommo::Translated(dropped.message, content),
            "De la Schelet: 20 de Oase și 5 galbeni.");
  const nanommo::Sentence full =
      nanommo::Msg("Your bag is full. Make room for $0 first.",
                   nanommo::Of(content.items, 6, 1));
  EXPECT_EQ(nanommo::Translated(full.message, content),
            "Rucsacul e plin. Întâi trebuie făcut loc pentru o Lamă de "
            "cenușă.");
  // A faction, by its id, whose name stays as it is, and its herald.
  const nanommo::factions::Faction& concordia = content.factions.at(1);
  EXPECT_EQ(
      nanommo::Translated(
          nanommo::Msg("You've joined $0.", nanommo::Of(concordia)).message,
          content),
      "Intrare în facțiunea Concordia.");
  EXPECT_EQ(
      nanommo::Translated(
          nanommo::Msg("You're too far away to talk to the $0.",
                       nanommo::Of(content.npcs.kinds.at(concordia.herald())))
              .message,
          content),
      "Prea departe de Vestitorul facțiunii Concordia pentru o discuție.");
}

TEST_F(Messages, KeepTheNamesPlayersGave) {
  InRomanian();
  const nanommo::Sentence asked =
      nanommo::Msg("$0 asks you to join $1.", nanommo::Name("alice"),
                   nanommo::Name("Iron Wolves"));
  EXPECT_EQ(nanommo::Translated(asked.message),
            "Invitație de la alice în Iron Wolves.");
}

TEST_F(Messages, SayWhatTheContentHasntGotIsSomething) {
  nanommo::protocol::Message message;
  message.set_text("You made $0.");
  message.add_args()->set_item(99);
  EXPECT_EQ(nanommo::Translated(message, content_), "You made something.");
  // And without the content, there's nothing to name.
  message.mutable_args(0)->set_item(4);
  EXPECT_EQ(nanommo::Translated(message), "You made something.");
}

}  // namespace
