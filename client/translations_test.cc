#include "client/translations.h"

#include <string>

#include "gtest/gtest.h"

namespace {

constexpr char kPo[] = R"po(# A translator's comment.
msgid ""
msgstr ""
"Language: ro\n"
"Plural-Forms: nplurals=3; plural=(n==1 ? 0 : (n==0 || (n%100 > 0 && n%100 < 20)) ? 1 : 2);\n"

#: client/menu.cc
msgid "Settings"
msgstr "Setări"

msgctxt "item 1"
msgid "Sword"
msgstr "Sabie"

msgid ""
"Two lines, "
"and a \"quote\".\n"
msgstr "Două rânduri\n"
"și un „citat”.\n"

msgid "$0 asks you to join $1."
msgstr "Invitație de la $0 în $1."

msgid "$0 gold"
msgid_plural "$0 gold"
msgstr[0] "$0 galben"
msgstr[1] "$0 galbeni"
msgstr[2] "$0 de galbeni"

#, fuzzy
msgid "Bag"
msgstr "Rucsac"

msgid "Quit"
msgstr ""

msgid "$0 playing"
msgstr "$1 joacă"

#~ msgid "Gone"
#~ msgstr "Dus"
)po";

// The interface goes back to English after each test, as the other tests'
// words are English.
class Translations : public testing::Test {
 protected:
  void TearDown() override { ASSERT_TRUE(nanommo::UseLanguage("", "").ok()); }
};

TEST(Catalog, ReadsAPoFile) {
  const absl::StatusOr<nanommo::Catalog> catalog = nanommo::Catalog::Parse(kPo);
  ASSERT_TRUE(catalog.ok()) << catalog.status();
  EXPECT_STREQ(catalog->Find("", "Settings"), "Setări");
  // In its context, and only there.
  EXPECT_STREQ(catalog->Find("item 1", "Sword"), "Sabie");
  EXPECT_EQ(catalog->Find("", "Sword"), nullptr);
  // Strings go on over lines, with their escapes.
  EXPECT_STREQ(catalog->Find("", "Two lines, and a \"quote\".\n"),
               "Două rânduri\nși un „citat”.\n");
  EXPECT_STREQ(catalog->Find("", "$0 asks you to join $1."),
               "Invitație de la $0 în $1.");
  // What's fuzzy, untranslated or obsolete isn't there, so it stays English.
  EXPECT_EQ(catalog->Find("", "Bag"), nullptr);
  EXPECT_EQ(catalog->Find("", "Quit"), nullptr);
  EXPECT_EQ(catalog->Find("", "Gone"), nullptr);
}

TEST(Catalog, LeavesOutAPlaceholderTheEnglishHasnt) {
  // absl::Substitute would have no $1 to fill in.
  const absl::StatusOr<nanommo::Catalog> catalog = nanommo::Catalog::Parse(kPo);
  ASSERT_TRUE(catalog.ok()) << catalog.status();
  EXPECT_EQ(catalog->Find("", "$0 playing"), nullptr);
}

TEST(Catalog, SaysWhichLineIsWrong) {
  const absl::StatusOr<nanommo::Catalog> unquoted =
      nanommo::Catalog::Parse("msgid \"Bag\"\nmsgstr Rucsac\n");
  ASSERT_FALSE(unquoted.ok());
  EXPECT_EQ(unquoted.status().message(), "line 2: expected a string in quotes");
  EXPECT_FALSE(nanommo::Catalog::Parse("msgid \"Bag\"\n").ok());
  EXPECT_FALSE(nanommo::Catalog::Parse("msgstr \"Rucsac\"\n").ok());
  EXPECT_FALSE(nanommo::Catalog::Parse("msgid \"\\q\"\nmsgstr \"\"\n").ok());
}

TEST(Catalog, PicksAPluralsFormByTheCount) {
  const absl::StatusOr<nanommo::Catalog> catalog = nanommo::Catalog::Parse(kPo);
  ASSERT_TRUE(catalog.ok()) << catalog.status();
  EXPECT_STREQ(catalog->Find("", "$0 gold", 1), "$0 galben");
  EXPECT_STREQ(catalog->Find("", "$0 gold", 3), "$0 galbeni");
  EXPECT_STREQ(catalog->Find("", "$0 gold", 20), "$0 de galbeni");
}

TEST(PluralRule, PicksRomaniansForms) {
  const absl::StatusOr<nanommo::PluralRule> rule = nanommo::PluralRule::Parse(
      "nplurals=3; plural=(n==1 ? 0 : (n==0 || (n%100 > 0 && n%100 < 20)) ? 1 "
      ": 2);");
  ASSERT_TRUE(rule.ok()) << rule.status();
  EXPECT_EQ(rule->forms(), 3);
  // 1 galben, 0 galbeni, 19 galbeni, 20 de galbeni, 101 galbeni, 100 de
  // galbeni.
  EXPECT_EQ(rule->Form(1), 0);
  EXPECT_EQ(rule->Form(0), 1);
  EXPECT_EQ(rule->Form(19), 1);
  EXPECT_EQ(rule->Form(20), 2);
  EXPECT_EQ(rule->Form(101), 1);
  EXPECT_EQ(rule->Form(100), 2);
  // And English's, by default.
  EXPECT_EQ(nanommo::PluralRule().Form(1), 0);
  EXPECT_EQ(nanommo::PluralRule().Form(2), 1);
}

TEST(PluralRule, RefusesARuleItCantFollow) {
  EXPECT_FALSE(nanommo::PluralRule::Parse("nplurals=2; plural=n >;").ok());
  EXPECT_FALSE(nanommo::PluralRule::Parse("nplurals=2; plural=(n != 1;").ok());
  EXPECT_FALSE(nanommo::PluralRule::Parse("nplurals=2; plural=n % 0;").ok());
  // n itself is a form for every count, of which there are only two.
  EXPECT_FALSE(nanommo::PluralRule::Parse("nplurals=2; plural=n;").ok());
  EXPECT_TRUE(nanommo::PluralRule::Parse("nplurals=1; plural=0;").ok());
}

TEST(Pseudo, MarksTheLettersAndMakesItLonger) {
  EXPECT_EQ(nanommo::Pseudo("Settings"), "[Șéțțîñgș ~~~]");
  // Placeholders stay, for absl::Substitute.
  EXPECT_EQ(nanommo::Pseudo("$0 gold"), "[$0 göld ~~]");
}

TEST_F(Translations, AreEnglishUntilALanguageIsPicked) {
  const char* text = "Settings";
  EXPECT_EQ(nanommo::Tr(text), text);
  EXPECT_STREQ(nanommo::TrN("$0 slot", "$0 slots", 1), "$0 slot");
  EXPECT_STREQ(nanommo::TrN("$0 slot", "$0 slots", 2), "$0 slots");
  EXPECT_STREQ(nanommo::TrLabel("Mute"), "Mute");
}

TEST_F(Translations, ComeFromTheLanguagesFile) {
  ASSERT_TRUE(nanommo::UseLanguage("ro", NANOMMO_PO).ok());
  EXPECT_STRNE(nanommo::Tr("Settings"), "Settings");
  // What has no translation stays English.
  EXPECT_STREQ(nanommo::Tr("Nothing translates this."),
               "Nothing translates this.");
  // A label keeps its English as its ID.
  EXPECT_EQ(std::string(nanommo::TrLabel("Mute")),
            std::string(nanommo::Tr("Mute")) + "###Mute");
}

TEST_F(Translations, ComeFromTheTemplateInPseudo) {
  ASSERT_TRUE(nanommo::UseLanguage(nanommo::kPseudo, NANOMMO_PO).ok());
  EXPECT_EQ(nanommo::Tr("Settings"), nanommo::Pseudo("Settings"));
  // What isn't in the template shows as it is, which is how pseudo shows
  // what nobody marked for translation.
  EXPECT_STREQ(nanommo::Tr("Nothing translates this."),
               "Nothing translates this.");
}

TEST_F(Translations, StayEnglishWithoutTheLanguagesFile) {
  EXPECT_FALSE(nanommo::UseLanguage("xx", NANOMMO_PO).ok());
  EXPECT_STREQ(nanommo::Tr("Settings"), "Settings");
}

}  // namespace
