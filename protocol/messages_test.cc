#include "protocol/messages.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"

namespace {

nanommo::items::Item Item(uint32_t id, const std::string& name) {
  nanommo::items::Item item;
  item.set_id(id);
  item.set_name(name);
  return item;
}

TEST(Messages, FillEachPlaceholderWithItsArg) {
  const std::vector<std::string> args = {"alice", "Iron Wolves"};
  EXPECT_EQ(nanommo::Fill("$1 asks $0 in.", args),
            "Iron Wolves asks alice in.");
  EXPECT_EQ(nanommo::Fill("$$5 and $0", args), "$5 and alice");
  // A placeholder without an arg comes out as nothing, and a $ before
  // anything else is a $.
  EXPECT_EQ(nanommo::Fill("$0, $7 and $x$", args), "alice,  and $x$");
}

TEST(Messages, AreTheirEnglishWithTheirArgs) {
  const nanommo::Sentence full =
      nanommo::Msg("Your bag is full. Make room for $0 first.",
                   nanommo::Of(Item(3, "Bone"), 3));
  EXPECT_EQ(full.english, "Your bag is full. Make room for 3 Bones first.");
  EXPECT_EQ(full.message.text(), "Your bag is full. Make room for $0 first.");
  ASSERT_EQ(full.message.args_size(), 1);
  EXPECT_EQ(full.message.args(0).item(), 3);
  EXPECT_EQ(full.message.args(0).count(), 3);
  // One of an item has its article, and its name alone has none.
  EXPECT_EQ(nanommo::Of(Item(3, "Bone"), 1).english, "a Bone");
  EXPECT_EQ(nanommo::Of(Item(6, "Ash Blade"), 1).english, "an Ash Blade");
  EXPECT_EQ(nanommo::Of(Item(6, "Ash Blade")).english, "Ash Blade");
  // Trousers, shoes and boots come in pairs.
  EXPECT_EQ(nanommo::Of(Item(9, "Cloth Trousers"), 1).english,
            "a pair of Cloth Trousers");
  EXPECT_EQ(nanommo::Of(Item(9, "Cloth Trousers"), 2).english,
            "2 pairs of Cloth Trousers");
  // A name stays as it is, and a number is a number.
  EXPECT_EQ(nanommo::Msg("$0 has $1.", nanommo::Name("alice"), 7).english,
            "alice has 7.");
}

TEST(Messages, PickAPluralsFormByTheCount) {
  const auto gold = [](uint64_t count) {
    return nanommo::MsgN("$0 gold", "$0 golds", count, count);
  };
  EXPECT_EQ(gold(1).english, "1 gold");
  EXPECT_EQ(gold(0).english, "0 golds");
  EXPECT_EQ(gold(5).message.count(), 5);
  EXPECT_EQ(gold(5).message.plural(), "$0 golds");
}

TEST(Messages, SayWhyInARefusal) {
  const absl::Status refused = nanommo::Refusal(
      absl::StatusCode::kResourceExhausted,
      nanommo::Msg("Make room for $0.", nanommo::Of(Item(3, "Bone"), 2)));
  EXPECT_TRUE(absl::IsResourceExhausted(refused));
  EXPECT_EQ(refused.message(), "Make room for 2 Bones.");
  const nanommo::Sentence why = nanommo::SentenceOf(refused);
  EXPECT_EQ(why.message.text(), "Make room for $0.");
  EXPECT_EQ(why.english, "Make room for 2 Bones.");
  // A status that isn't a refusal says what its message says.
  const nanommo::Sentence other =
      nanommo::SentenceOf(absl::InternalError("a grade that isn't a rank"));
  EXPECT_EQ(other.message.text(), "a grade that isn't a rank");
  EXPECT_EQ(other.message.args_size(), 0);
}

TEST(Messages, AreTheSameInEnglishFromTheContent) {
  nanommo::Content content;
  content.items.by_id[3] = Item(3, "Bone");
  const nanommo::Sentence sentence = nanommo::Msg(
      "You made $0, from $1, and $2.", nanommo::Of(content.items, 3, 1),
      nanommo::Of(content.items, 9, 1),
      nanommo::Msg("$0 bit", nanommo::Of(nanommo::items::SLOT_HEAD)));
  EXPECT_EQ(sentence.english, "You made a Bone, from something, and head bit.");
  EXPECT_EQ(nanommo::English(sentence.message, content), sentence.english);
}

}  // namespace
