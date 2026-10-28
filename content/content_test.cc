#include "content/content.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"

TEST(Content, ReadsTheGamesContent) {
  const absl::StatusOr<nanommo::Content> content =
      nanommo::ReadContent(NANOMMO_CONTENT);
  ASSERT_TRUE(content.ok()) << content.status();

  // A new player has a sword in hand, and potions to drink.
  const nanommo::items::Item& sword =
      content->items.by_id.at(content->items.starting.front().item());
  EXPECT_EQ(sword.name(), "Sword");
  EXPECT_GT(sword.weapon().damage(), 0u);
  EXPECT_EQ(sword.equip().slot(), nanommo::items::SLOT_MAIN_HAND);
  EXPECT_FALSE(sword.look().model().empty());
  EXPECT_TRUE(
      std::ranges::any_of(content->items.starting, [&](const auto& kit) {
        return content->items.by_id.at(kit.item()).use().heal() > 0;
      }));
  EXPECT_TRUE(std::ranges::any_of(content->items.by_id, [](const auto& item) {
    return nanommo::StackSize(item.second) > 1;
  }));
  EXPECT_TRUE(std::ranges::any_of(content->items.by_id, [](const auto& item) {
    return !item.second.rolls().empty();
  }));
  EXPECT_FALSE(content->figures.body().empty());
  const nanommo::monsters::Kind& skeleton = content->monsters.kinds.at(1);
  EXPECT_EQ(skeleton.name(), "Skeleton");
  EXPECT_FALSE(content->monsters.spawns.empty());
  EXPECT_TRUE(std::ranges::any_of(content->npcs.kinds, [](const auto& kind) {
    return kind.second.talks();
  }));
  EXPECT_TRUE(std::ranges::any_of(content->skills.by_id, [](const auto& skill) {
    return skill.second.level() == 1;
  }));
  EXPECT_FALSE(content->skills.starting.empty());
  EXPECT_TRUE(std::ranges::any_of(content->npcs.kinds, [](const auto& kind) {
    return !kind.second.teaches().empty();
  }));
}

TEST(Content, HashesWhatTheClientsRead) {
  nanommo::Content content = *nanommo::ReadContent(NANOMMO_CONTENT);
  const uint64_t hash = nanommo::ClientHash(content);

  content.items.by_id.at(1).mutable_weapon()->set_damage(1000);
  content.skills.by_id.at(1).mutable_effect()->set_damage(1000);
  EXPECT_EQ(nanommo::ClientHash(content), hash);
  content.items.by_id.at(1).mutable_look()->set_length(2.0f);
  EXPECT_NE(nanommo::ClientHash(content), hash);

  // The bag shows what an item adds, so that's the clients' too.
  nanommo::Content stronger = *nanommo::ReadContent(NANOMMO_CONTENT);
  stronger.items.by_id.at(4).mutable_equip()->mutable_modifiers(0)->set_amount(
      9);
  EXPECT_NE(nanommo::ClientHash(stronger), hash);
}

TEST(Content, RefusesFiguresThatDontAddUp) {
  constexpr char kArm[] =
      "body { shape: SHAPE_LIMB bone: \"LeftArm\" to: \"LeftForeArm\" "
      "start_radius: 0.06 end_radius: 0.05 }";
  EXPECT_TRUE(nanommo::ParseFigures(kArm).ok());
  EXPECT_EQ(nanommo::ParseFigures("body { shape: SHAPE_LIMB to: \"Head\" }")
                .status()
                .message(),
            "a part has no bone to move with");
  EXPECT_EQ(nanommo::ParseFigures("body { bone: \"Head\" }").status().message(),
            "a part on the Head has no shape");
  EXPECT_EQ(nanommo::ParseFigures("body { shape: SHAPE_LIMB bone: \"LeftArm\" "
                                  "to: \"LeftForeArm\" start_radius: 0.06 }")
                .status()
                .message(),
            "a limb on the LeftArm needs an end, and a radius at each");
  EXPECT_TRUE(nanommo::ParseFigures("body { shape: SHAPE_BALL bone: \"Head\" "
                                    "size { x: 0.2 y: 0.2 z: 0.2 } }")
                  .ok());
  EXPECT_EQ(nanommo::ParseFigures("body { shape: SHAPE_BALL bone: \"Head\" "
                                  "size { x: 0.2 y: 0.2 } }")
                .status()
                .message(),
            "a ball on the Head needs a size each way");
  EXPECT_TRUE(nanommo::ParseFigures("hairs { parts { shape: SHAPE_BALL "
                                    "bone: \"Head\" size { x: 0.2 y: 0.1 "
                                    "z: 0.2 } } }")
                  .ok());
  EXPECT_EQ(nanommo::ParseFigures("hairs { parts { shape: SHAPE_BALL "
                                  "bone: \"Head\" size { x: 0.2 y: 0.1 } } }")
                .status()
                .message(),
            "a way to wear the hair: a ball on the Head needs a size each way");
}

TEST(Content, PicksEachPlayersFeaturesFromTheFigures) {
  const nanommo::figures::Figures figures = *nanommo::ParseFigures(
      "skins: 0xF1C49CFF skins: 0x80543AFF hair_colors: 0x1C181AFF "
      "hairs {} hairs {} hairs {}");

  // Over the first few players, each skin and each way to wear the hair,
  // and the one hair colour.
  std::set<uint32_t> skins;
  std::set<int> hairs;
  for (uint32_t id = 1; id <= 8; ++id) {
    const nanommo::Features features = nanommo::FeaturesOf(figures, id);
    skins.insert(features.skin);
    hairs.insert(features.hair);
    EXPECT_EQ(features.hair_color, 0x1C181AFFu);
  }
  EXPECT_EQ(skins, (std::set<uint32_t>{0xF1C49CFF, 0x80543AFF}));
  EXPECT_EQ(hairs, (std::set<int>{0, 1, 2}));

  // With none to pick from, white, and no hair.
  const nanommo::Features none = nanommo::FeaturesOf({}, 1);
  EXPECT_EQ(none.skin, 0xFFFFFFFFu);
  EXPECT_EQ(none.hair, -1);
}

TEST(Content, UnfoldsMirroredPartsOntoTheRight) {
  const nanommo::figures::Figures figures = *nanommo::ParseFigures(
      "body { shape: SHAPE_BALL bone: \"LeftHand\" from_offset { x: 0.04 } "
      "size { x: 0.07 y: 0.07 z: 0.07 } mirrored: true } "
      "body { shape: SHAPE_BALL bone: \"Head\" "
      "size { x: 0.2 y: 0.2 z: 0.2 } }");

  const std::vector<nanommo::figures::Part> parts =
      nanommo::Unfold(figures.body());

  ASSERT_EQ(parts.size(), 3u);
  EXPECT_EQ(parts[0].bone(), "LeftHand");
  EXPECT_EQ(parts[1].bone(), "RightHand");
  EXPECT_FLOAT_EQ(parts[1].from_offset().x(), -0.04f);
  EXPECT_EQ(parts[2].bone(), "Head");
}

TEST(Content, RefusesItemsThatDontAddUp) {
  constexpr char kSword[] =
      "items { id: 1 name: \"Sword\" weapon {} "
      "equip { slot: SLOT_MAIN_HAND } } ";
  EXPECT_TRUE(nanommo::ParseItems(kSword).ok());
  EXPECT_EQ(nanommo::ParseItems("items { id: 1 weapon { reech: 2 } }")
                .status()
                .message(),
            "line 1: Message type \"nanommo.items.Weapon\" has no field named "
            "\"reech\".");
  EXPECT_FALSE(nanommo::ParseItems(absl::StrCat(kSword, kSword)).ok());
  EXPECT_EQ(nanommo::ParseItems("items { id: 1 name: \"Sword\" weapon {} "
                                "equip { slot: SLOT_HEAD } }")
                .status()
                .message(),
            "the Sword is a weapon, so it's held in the main hand");
  EXPECT_EQ(nanommo::ParseItems("items { id: 1 name: \"Cap\" equip {} }")
                .status()
                .message(),
            "the Cap is worn, but has no slot to be worn in");

  // What it looks like on a player.
  EXPECT_TRUE(nanommo::ParseItems("items { id: 1 name: \"Cap\" look { parts { "
                                  "shape: SHAPE_BALL bone: \"Head\" "
                                  "size { x: 0.2 y: 0.1 z: 0.2 } } } }")
                  .ok());
  EXPECT_EQ(nanommo::ParseItems("items { id: 1 name: \"Cap\" look { parts { "
                                "shape: SHAPE_BALL bone: \"Head\" } } }")
                .status()
                .message(),
            "the Cap's look: a ball on the Head needs a size each way");

  // Stacks, and the rolls of what doesn't stack.
  EXPECT_TRUE(
      nanommo::ParseItems("items { id: 1 name: \"Bone\" stack: 50 }").ok());
  EXPECT_EQ(nanommo::ParseItems("items { id: 1 name: \"Bone\" stack: 50 "
                                "rolls { stat: STAT_DAMAGE max: 2 } }")
                .status()
                .message(),
            "the Bone stacks, so it can't be worn, or roll its own");
  EXPECT_TRUE(nanommo::ParseItems("items { id: 1 name: \"Blade\" "
                                  "rolls { stat: STAT_DAMAGE max: 2 } }")
                  .ok());
  EXPECT_EQ(nanommo::ParseItems("items { id: 1 name: \"Blade\" "
                                "rolls { stat: STAT_DAMAGE min: 3 max: 2 } }")
                .status()
                .message(),
            "the Blade rolls from 3 to 2 on STAT_DAMAGE");

  // The kit, and the ids that are gone.
  EXPECT_TRUE(
      nanommo::ParseItems(absl::StrCat(kSword, "starting { item: 1 }")).ok());
  EXPECT_EQ(nanommo::ParseItems(absl::StrCat(kSword, "starting { item: 2 }"))
                .status()
                .message(),
            "there's no item 2 to start with");
  EXPECT_EQ(
      nanommo::ParseItems(absl::StrCat(kSword, "starting { item: 1 count: 2 }"))
          .status()
          .message(),
      "a new player starts with 2 of the Sword, which don't fit in a "
      "slot");
  EXPECT_TRUE(nanommo::ParseItems(absl::StrCat(kSword, "retired: 2")).ok());
  EXPECT_EQ(nanommo::ParseItems(absl::StrCat(kSword, "retired: 1"))
                .status()
                .message(),
            "the Sword has the id 1, which is retired");
}

TEST(Content, RefusesMonstersThatDontAddUp) {
  EXPECT_TRUE(
      nanommo::ParseMonsters("kinds { id: 1 } "
                             "spawns { kind: 1 land: \"Meadows\" x: 5 }")
          .ok());
  EXPECT_FALSE(nanommo::ParseMonsters("kinds { id: 1 } "
                                      "spawns { kind: 2 land: \"Meadows\" }")
                   .ok());
  EXPECT_FALSE(nanommo::ParseMonsters("kinds { id: 1 } "
                                      "camps { kind: 1 land: \"Marsh\" }")
                   .ok());
  EXPECT_FALSE(
      nanommo::ParseMonsters("kinds { id: 1 fight { leash: 6 } } "
                             "spawns { kind: 1 land: \"Meadows\" x: 5 }")
          .ok());
}

TEST(Content, RefusesLootThatDoesntAddUp) {
  EXPECT_TRUE(
      nanommo::ParseMonsters("kinds { id: 1 name: \"Skeleton\" "
                             "loot { item: 3 chance: 0.5 min: 1 max: 3 } }")
          .ok());
  EXPECT_EQ(nanommo::ParseMonsters("kinds { id: 1 name: \"Skeleton\" "
                                   "loot { item: 3 } }")
                .status()
                .message(),
            "the Skeleton drops item 3 with a chance of 0, which has to be "
            "more than 0 and at most 1");
  EXPECT_FALSE(nanommo::ParseMonsters("kinds { id: 1 name: \"Skeleton\" "
                                      "loot { item: 3 chance: 1.5 } }")
                   .ok());
  EXPECT_EQ(
      nanommo::ParseMonsters("kinds { id: 1 name: \"Skeleton\" "
                             "loot { item: 3 chance: 0.5 min: 3 max: 1 } }")
          .status()
          .message(),
      "the Skeleton drops from 3 to 1 of item 3");

  // What a monster drops has to be in the items file.
  nanommo::Content content = *nanommo::ReadContent(NANOMMO_CONTENT);
  EXPECT_TRUE(std::ranges::any_of(content.monsters.kinds, [](const auto& kind) {
    return !kind.second.loot().empty();
  }));
  nanommo::monsters::Drop* drop = content.monsters.kinds.at(1).add_loot();
  drop->set_item(99);
  drop->set_chance(0.5f);
  EXPECT_EQ(nanommo::CheckContent(content).message(),
            "the Skeleton drops an item 99 that isn't in the items file");
}

TEST(Content, RefusesNpcsThatDontAddUp) {
  EXPECT_FALSE(nanommo::ParseNpcs("kinds { id: 1 } "
                                  "spawns { kind: 2 land: \"Meadows\" }")
                   .ok());

  nanommo::Content content = *nanommo::ReadContent(NANOMMO_CONTENT);
  EXPECT_TRUE(nanommo::CheckContent(content).ok());
  content.npcs.kinds.at(1).add_teaches(99);
  EXPECT_EQ(nanommo::CheckContent(content).message(),
            "the Guard teaches a skill 99 that isn't in the skills file");
}

TEST(Content, RefusesSkillsThatDontAddUp) {
  EXPECT_TRUE(nanommo::ParseSkills("skills { id: 1 name: \"Kick\" level: 1 "
                                   "effect { cast_ms: 900 hit_ms: 300 } }")
                  .ok());
  EXPECT_EQ(nanommo::ParseSkills("skills { id: 1 name: \"Kick\" "
                                 "effect { cast_ms: 900 hit_ms: 300 } }")
                .status()
                .message(),
            "the Kick has no level");
  EXPECT_EQ(nanommo::ParseSkills("skills { id: 1 name: \"Kick\" level: 1 "
                                 "effect { cast_ms: 300 hit_ms: 900 } }")
                .status()
                .message(),
            "the Kick lands after it's over");
  EXPECT_EQ(nanommo::ParseSkills("starting: 2 skills { id: 1 name: \"Kick\" "
                                 "level: 1 effect { cast_ms: 900 } }")
                .status()
                .message(),
            "there's no skill 2 to start with");
}
