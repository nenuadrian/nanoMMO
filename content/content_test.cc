#include "content/content.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
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
  EXPECT_GT(skeleton.gold().max(), 0u);
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
  const nanommo::quests::Quest& bones = content->quests.at(1);
  EXPECT_EQ(content->npcs.kinds.at(bones.giver()).name(), "Guard");
  EXPECT_EQ(bones.kill().monster(), 1u);
  EXPECT_GT(bones.kill().count(), 0u);
  // The sword and bones make a better one.
  const nanommo::recipes::Recipe& bind = content->recipes.at(2);
  EXPECT_EQ(content->items.by_id.at(bind.inputs(0).item()).name(), "Sword");
  EXPECT_EQ(content->items.by_id.at(bind.inputs(1).item()).name(), "Bone");
  EXPECT_EQ(content->items.by_id.at(bind.output().item()).name(), "Bone Sword");
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

  // The quest window shows all of a quest, and the crafting window all of a
  // recipe.
  nanommo::Content quests = *nanommo::ReadContent(NANOMMO_CONTENT);
  quests.quests.at(1).set_experience(1000);
  EXPECT_NE(nanommo::ClientHash(quests), hash);
  nanommo::Content recipes = *nanommo::ReadContent(NANOMMO_CONTENT);
  recipes.recipes.at(2).set_seconds(9);
  EXPECT_NE(nanommo::ClientHash(recipes), hash);

  // They draw a monster's skill's shape, and not what it does.
  nanommo::Content slam = *nanommo::ReadContent(NANOMMO_CONTENT);
  slam.monsters.kinds.at(2).mutable_skills(0)->set_damage(1000);
  EXPECT_EQ(nanommo::ClientHash(slam), hash);
  slam.monsters.kinds.at(2).mutable_skills(0)->set_size(9.0f);
  EXPECT_NE(nanommo::ClientHash(slam), hash);

  // They mark where a boss's phases start, and not what it does in them.
  nanommo::Content boss = *nanommo::ReadContent(NANOMMO_CONTENT);
  boss.monsters.kinds.at(4).mutable_phases(1)->add_skills(1);
  EXPECT_EQ(nanommo::ClientHash(boss), hash);
  boss.monsters.kinds.at(4).mutable_phases(1)->set_health(50);
  EXPECT_NE(nanommo::ClientHash(boss), hash);

  // The clients work out how the people move from the rig.
  nanommo::Content rig = *nanommo::ReadContent(NANOMMO_CONTENT);
  rig.rig.mutable_walk()->set_cycle_ms(900);
  EXPECT_NE(nanommo::ClientHash(rig), hash);
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

TEST(Content, RefusesRigsThatDontAddUp) {
  constexpr char kLegs[] =
      "joints { name: \"Hips\" at { y: 1 } } "
      "joints { name: \"LeftUpLeg\" parent: \"Hips\" "
      "at { x: 0.1 y: 0.95 } mirrored: true } ";
  constexpr char kWalk[] = "walk { speed: 1.8 cycle_ms: 1000 stance: 0.6 } ";
  constexpr char kRun[] = "run { speed: 3.1 cycle_ms: 750 stance: 0.4 } ";
  EXPECT_TRUE(nanommo::ParseRig(absl::StrCat(kLegs, kWalk, kRun)).ok());
  auto refused = [&](const std::string& text) {
    return std::string(nanommo::ParseRig(text).status().message());
  };
  EXPECT_EQ(refused(absl::StrCat("joints { name: \"LeftUpLeg\" parent: "
                                 "\"Hips\" } joints { name: \"Hips\" } ",
                                 kWalk, kRun)),
            "the LeftUpLeg hangs from a joint that isn't before it");
  EXPECT_EQ(
      refused(absl::StrCat(kLegs, "joints { name: \"Tail\" } ", kWalk, kRun)),
      "the Tail hangs from a joint that isn't before it");
  EXPECT_EQ(refused(absl::StrCat(kLegs,
                                 "joints { name: \"RightUpLeg\" parent: "
                                 "\"Hips\" } ",
                                 kWalk, kRun)),
            "there are two joints called RightUpLeg");
  EXPECT_EQ(refused(absl::StrCat(kLegs,
                                 "builds { shifts { joint: \"LeftArm\" "
                                 "by { x: 0.03 } } } ",
                                 kWalk, kRun)),
            "a build moves a joint LeftArm the rig doesn't have");
  EXPECT_TRUE(
      nanommo::ParseRig(absl::StrCat(kLegs,
                                     "builds { shifts { joint: "
                                     "\"RightUpLeg\" by { x: 0.01 } } }",
                                     kWalk, kRun))
          .ok());
  EXPECT_EQ(refused(absl::StrCat(
                kLegs, "walk { speed: 1.8 cycle_ms: 1000 stance: 1 } ", kRun)),
            "the walk's feet are down for none of its cycle, or all of it");
  EXPECT_EQ(
      refused(absl::StrCat(kLegs, kWalk, "run { cycle_ms: 750 stance: 0.4 }")),
      "the run needs a speed and a cycle");
  EXPECT_EQ(refused(absl::StrCat(kLegs, kWalk, kRun,
                                 "idle { keys { at_ms: 500 } "
                                 "keys { at_ms: 100 } }")),
            "the idle goes back in time");

  // Each part of a figure, or of what it wears, is on a joint of the rig.
  nanommo::Content content = *nanommo::ReadContent(NANOMMO_CONTENT);
  EXPECT_TRUE(nanommo::CheckContent(content).ok());
  content.items.by_id.at(10).mutable_look()->mutable_parts(1)->set_to(
      "LeftToes");
  EXPECT_EQ(nanommo::CheckContent(content).message(),
            "the Plain Shoes has a part on a joint LeftToes that isn't in the "
            "rig");
  content = *nanommo::ReadContent(NANOMMO_CONTENT);
  content.figures.mutable_body(0)->set_bone("Tail");
  EXPECT_EQ(nanommo::CheckContent(content).message(),
            "the body has a part on a joint Tail that isn't in the rig");
}

TEST(Content, BuildsEachOfTheRigsBuilds) {
  const nanommo::rig::Rig rig = *nanommo::ParseRig(
      "joints { name: \"Hips\" at { y: 1 } } "
      "joints { name: \"LeftArm\" parent: \"Hips\" at { x: 0.2 y: 1.4 } "
      "mirrored: true } "
      "joints { name: \"LeftHand\" parent: \"LeftArm\" "
      "at { x: 0.7 y: 1.4 } mirrored: true } "
      "builds {} "
      "builds { shifts { joint: \"LeftArm\" by { x: 0.03 } } } "
      "walk { speed: 1.8 cycle_ms: 1000 stance: 0.6 } "
      "run { speed: 3.1 cycle_ms: 750 stance: 0.4 }");

  // Each twin on the right comes after its own on the left, and hangs from
  // the right's.
  const std::vector<nanommo::rig::Joint> joints = nanommo::Skeleton(rig, 0);
  ASSERT_EQ(joints.size(), 5u);
  EXPECT_EQ(joints[2].name(), "RightArm");
  EXPECT_EQ(joints[2].parent(), "Hips");
  EXPECT_EQ(joints[4].name(), "RightHand");
  EXPECT_EQ(joints[4].parent(), "RightArm");
  EXPECT_FLOAT_EQ(joints[4].at().x(), -0.7f);

  // The second build's shoulders are wider, and so are its hands, which
  // hang from them, and the hips stay where they are.
  const std::vector<nanommo::rig::Joint> broad = nanommo::Skeleton(rig, 1);
  EXPECT_FLOAT_EQ(broad[0].at().x(), 0.0f);
  EXPECT_FLOAT_EQ(broad[1].at().x(), 0.23f);
  EXPECT_FLOAT_EQ(broad[2].at().x(), -0.23f);
  EXPECT_FLOAT_EQ(broad[3].at().x(), 0.73f);
  EXPECT_FLOAT_EQ(broad[4].at().x(), -0.73f);
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
  EXPECT_EQ(nanommo::ParseMonsters("kinds { id: 1 name: \"Skeleton\" "
                                   "gold { min: 9 max: 2 } }")
                .status()
                .message(),
            "the Skeleton drops from 9 to 2 gold");

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

TEST(Content, ReadsTheMonstersSkills) {
  const nanommo::Content content = *nanommo::ReadContent(NANOMMO_CONTENT);
  // The plain skeleton only swings, so that new players learn that first.
  EXPECT_TRUE(content.monsters.kinds.at(1).skills().empty());

  const nanommo::monsters::Skill& slam = content.monsters.kinds.at(2).skills(0);
  EXPECT_EQ(slam.name(), "Ground Slam");
  EXPECT_EQ(slam.shape(), nanommo::monsters::CIRCLE);
  EXPECT_GT(slam.damage(), content.monsters.kinds.at(2).fight().damage());
  const nanommo::monsters::Skill& sweep =
      content.monsters.kinds.at(3).skills(0);
  EXPECT_EQ(sweep.name(), "Bone Sweep");
  EXPECT_EQ(sweep.shape(), nanommo::monsters::CONE);
  EXPECT_GT(sweep.arc(), 0.0f);
}

TEST(Content, RefusesMonsterSkillsThatDontAddUp) {
  const auto slam = [](std::string_view skill) {
    return nanommo::ParseMonsters(
               absl::StrCat("kinds { id: 1 name: \"Ash Skeleton\" skills { ",
                            "id: 1 name: \"Slam\" ", skill, " } }"))
        .status();
  };
  // Seen for half a second, from a metre and a half, in a circle of three.
  EXPECT_TRUE(
      slam("size: 3 range: 1.5 telegraph_ms: 500 cooldown_ms: 500").ok());
  EXPECT_EQ(slam("range: 1.5 telegraph_ms: 500 cooldown_ms: 500").message(),
            "the Ash Skeleton's Slam has no size");
  EXPECT_EQ(
      slam("shape: CONE size: 3 range: 1.5 telegraph_ms: 500 cooldown_ms: 500")
          .message(),
      "the Ash Skeleton's Slam is a cone without an arc");
  EXPECT_TRUE(slam("shape: CONE size: 3 arc: 90 range: 1.5 telegraph_ms: 500 "
                   "cooldown_ms: 500")
                  .ok());
  EXPECT_EQ(slam("size: 3 telegraph_ms: 500 cooldown_ms: 500").message(),
            "the Ash Skeleton's Slam has no range");
  EXPECT_EQ(
      slam("size: 3 range: 4 telegraph_ms: 500 cooldown_ms: 500").message(),
      "the Ash Skeleton's Slam starts from 4 metres, outside its 3");
  EXPECT_EQ(
      slam("size: 3 range: 1.5 telegraph_ms: 499 cooldown_ms: 500").message(),
      "the Ash Skeleton's Slam shows for 499 ms, too short to step out "
      "of: it needs 500 at least");
  EXPECT_EQ(
      slam("size: 3 range: 1.5 telegraph_ms: 2000 cooldown_ms: 1999").message(),
      "the Ash Skeleton's Slam cools down in 1999 ms, before it lands, "
      "at 2000");

  // Each has an id of its own among its kind's.
  EXPECT_EQ(nanommo::ParseMonsters(
                "kinds { id: 1 skills { name: \"Slam\" size: 3 range: 1.5 "
                "telegraph_ms: 500 cooldown_ms: 500 } }")
                .status()
                .message(),
            "the Slam has no id");
}

TEST(Content, ReadsTheBoss) {
  const nanommo::Content content = *nanommo::ReadContent(NANOMMO_CONTENT);
  const nanommo::monsters::Kind& colossus = content.monsters.kinds.at(4);
  EXPECT_EQ(colossus.name(), "Bone Colossus");
  // Its phases go down its health, each with more of its skills than the
  // last.
  ASSERT_EQ(colossus.phases_size(), 3);
  EXPECT_EQ(colossus.phases(0).health(), 100u);
  for (int i = 1; i < colossus.phases_size(); ++i) {
    EXPECT_LT(colossus.phases(i).health(), colossus.phases(i - 1).health());
    EXPECT_GT(colossus.phases(i).skills_size(),
              colossus.phases(i - 1).skills_size());
  }
  // It has one home, in the Meadows.
  EXPECT_EQ(std::ranges::count(content.monsters.spawns, 4u,
                               &nanommo::monsters::Spawn::kind),
            1);
}

TEST(Content, RefusesPhasesThatDontAddUp) {
  const auto boss = [](std::string_view phases) {
    return nanommo::ParseMonsters(
               absl::StrCat("kinds { id: 1 name: \"Colossus\" skills { id: 1 ",
                            "name: \"Slam\" size: 3 range: 1.5 telegraph_ms: ",
                            "500 cooldown_ms: 500 } ", phases, " }"))
        .status();
  };
  // From full health, to 60%, each with the slam.
  EXPECT_TRUE(boss("phases { health: 100 skills: 1 } "
                   "phases { health: 60 skills: 1 }")
                  .ok());
  EXPECT_EQ(boss("phases { health: 90 }").message(),
            "the Colossus's phase 1 starts at 90% of its health, not at 100%");
  EXPECT_EQ(boss("phases { health: 100 } phases { health: 0 }").message(),
            "the Colossus's phase 2 starts at 0%, when it's dead");
  EXPECT_EQ(boss("phases { health: 100 } phases { health: 60 } "
                 "phases { health: 60 }")
                .message(),
            "the Colossus's phase 3 starts at 60%, which isn't below phase "
            "2's 60%");
  EXPECT_EQ(boss("phases { health: 100 skills: 2 }").message(),
            "the Colossus's phase 1 uses a skill 2 it doesn't have");

  // It calls up some monsters, of a kind in the file.
  const auto calls = [](std::string_view adds) {
    return nanommo::ParseMonsters(
               absl::StrCat("kinds { id: 1 name: \"Colossus\" phases { ",
                            "health: 100 ", adds, " } } ",
                            "kinds { id: 2 name: \"Skeleton\" }"))
        .status();
  };
  EXPECT_TRUE(calls("adds { kind: 2 count: 2 }").ok());
  EXPECT_EQ(calls("adds { kind: 3 count: 2 }").message(),
            "the Colossus's phase 1 calls up a kind 3 that isn't in the file");
  EXPECT_EQ(calls("adds { kind: 2 }").message(),
            "the Colossus's phase 1 calls up no Skeletons");
}

TEST(Content, RefusesNpcsThatDontAddUp) {
  EXPECT_TRUE(nanommo::ParseNpcs("kinds { id: 1 look { skin: 0xF1C49CFF } } "
                                 "spawns { kind: 1 land: \"Meadows\" }")
                  .ok());
  EXPECT_FALSE(nanommo::ParseNpcs("kinds { id: 1 look { skin: 0xF1C49CFF } } "
                                  "spawns { kind: 2 land: \"Meadows\" }")
                   .ok());
  EXPECT_EQ(
      nanommo::ParseNpcs("kinds { id: 1 name: \"Guard\" }").status().message(),
      "the Guard has no skin");

  nanommo::Content content = *nanommo::ReadContent(NANOMMO_CONTENT);
  EXPECT_TRUE(nanommo::CheckContent(content).ok());
  content.npcs.kinds.at(1).mutable_look()->add_wearing(99);
  EXPECT_EQ(nanommo::CheckContent(content).message(),
            "the Guard wears an item 99 that isn't in the items file");
  content = *nanommo::ReadContent(NANOMMO_CONTENT);
  content.npcs.kinds.at(1).mutable_look()->set_hair(9);
  EXPECT_EQ(nanommo::CheckContent(content).message(),
            "the Guard has a hair 9 that isn't in the figures file");
  content = *nanommo::ReadContent(NANOMMO_CONTENT);
  content.npcs.kinds.at(1).add_teaches(99);
  EXPECT_EQ(nanommo::CheckContent(content).message(),
            "the Guard teaches a skill 99 that isn't in the skills file");
}

TEST(Content, RefusesQuestsThatDontAddUp) {
  EXPECT_TRUE(nanommo::ParseQuests(R"pb(quests {
                                          id: 1
                                          name: "Bones"
                                          giver: 1
                                          kill { monster: 1 count: 3 }
                                          prizes { item: 2 count: 2 }
                                        })pb")
                  .ok());
  EXPECT_EQ(nanommo::ParseQuests(R"pb(quests {
                                        id: 1
                                        name: "Bones"
                                        kill { monster: 1 count: 3 }
                                      })pb")
                .status()
                .message(),
            "the Bones has no giver");
  EXPECT_EQ(nanommo::ParseQuests(R"pb(quests {
                                        id: 1
                                        name: "Bones"
                                        giver: 1
                                        kill { monster: 1 }
                                      })pb")
                .status()
                .message(),
            "the Bones asks for no kills");
  EXPECT_EQ(nanommo::ParseQuests(R"pb(quests {
                                        id: 1
                                        name: "Bones"
                                        giver: 1
                                        kill { monster: 1 count: 3 }
                                        prizes { item: 2 }
                                      })pb")
                .status()
                .message(),
            "the Bones gives none of item 2");

  nanommo::Content content = *nanommo::ReadContent(NANOMMO_CONTENT);
  content.quests.at(1).set_giver(99);
  EXPECT_EQ(nanommo::CheckContent(content).message(),
            "the Bones by the Gate has a giver 99 that isn't in the NPCs file");
  content = *nanommo::ReadContent(NANOMMO_CONTENT);
  content.quests.at(1).mutable_kill()->set_monster(99);
  EXPECT_EQ(
      nanommo::CheckContent(content).message(),
      "the Bones by the Gate asks for a monster 99 that isn't in the monsters "
      "file");
  content = *nanommo::ReadContent(NANOMMO_CONTENT);
  content.quests.at(1).mutable_prizes(0)->set_item(99);
  EXPECT_EQ(nanommo::CheckContent(content).message(),
            "the Bones by the Gate gives an item 99 that isn't in the items "
            "file");
}

TEST(Content, RefusesRecipesThatDontAddUp) {
  // Twelve bones make a buckler, in five seconds, from level 1.
  constexpr char kTakes[] = "inputs { item: 3 count: 12 } ";
  constexpr char kMakes[] = "output { item: 12 count: 1 } ";
  const auto carve = [](std::string_view fields) {
    return nanommo::ParseRecipes(
        absl::StrCat("recipes { id: 1 name: \"Carve\" ", fields, " }"));
  };
  EXPECT_TRUE(carve(absl::StrCat(kTakes, kMakes, "seconds: 5 level: 1")).ok());
  EXPECT_EQ(
      carve(absl::StrCat(kTakes, kMakes, "seconds: 5")).status().message(),
      "Carve has no level");
  EXPECT_EQ(carve(absl::StrCat(kTakes, kMakes, "level: 1")).status().message(),
            "Carve takes no time");
  EXPECT_EQ(
      carve(absl::StrCat(kMakes, "seconds: 5 level: 1")).status().message(),
      "Carve takes nothing");
  EXPECT_EQ(
      carve(absl::StrCat("inputs { item: 3 } ", kMakes, "seconds: 5 level: 1"))
          .status()
          .message(),
      "Carve takes none of item 3");
  EXPECT_EQ(
      carve(absl::StrCat(kTakes, "output { item: 12 } seconds: 5 level: 1"))
          .status()
          .message(),
      "Carve makes none of item 12");

  // What it takes and makes is in the items file, a slot's worth of each.
  nanommo::Content content = *nanommo::ReadContent(NANOMMO_CONTENT);
  EXPECT_TRUE(nanommo::CheckContent(content).ok());
  content.recipes.at(2).mutable_inputs(0)->set_item(99);
  EXPECT_EQ(nanommo::CheckContent(content).message(),
            "Bind a Bone Sword takes an item 99 that isn't in the items file");
  content = *nanommo::ReadContent(NANOMMO_CONTENT);
  content.recipes.at(2).mutable_output()->set_item(99);
  EXPECT_EQ(nanommo::CheckContent(content).message(),
            "Bind a Bone Sword makes an item 99 that isn't in the items file");
  content = *nanommo::ReadContent(NANOMMO_CONTENT);
  content.recipes.at(2).mutable_inputs(1)->set_count(50);
  EXPECT_TRUE(nanommo::CheckContent(content).ok());
  content.recipes.at(2).mutable_inputs(1)->set_count(51);
  EXPECT_EQ(
      nanommo::CheckContent(content).message(),
      "Bind a Bone Sword takes 51 of the Bone, which don't fit in a slot");
  content = *nanommo::ReadContent(NANOMMO_CONTENT);
  content.recipes.at(2).mutable_output()->set_count(2);
  EXPECT_EQ(nanommo::CheckContent(content).message(),
            "Bind a Bone Sword makes 2 of the Bone Sword, which don't fit in a "
            "slot");
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
  EXPECT_EQ(nanommo::ParseSkills("skills { id: 1 name: \"Spin\" level: 1 "
                                 "effect { cast_ms: 900 blows { at_ms: 600 } "
                                 "blows { at_ms: 300 } } }")
                .status()
                .message(),
            "the Spin's blows go back in time");
  EXPECT_EQ(nanommo::ParseSkills("skills { id: 1 name: \"Spin\" level: 1 "
                                 "effect { cast_ms: 900 blows { at_ms: 300 } "
                                 "blows { at_ms: 1200 } } }")
                .status()
                .message(),
            "the Spin lands a blow after it's over");
  EXPECT_TRUE(nanommo::ParseSkills("skills { id: 1 name: \"Dash\" level: 1 "
                                   "effect { cast_ms: 900 strides { at_ms: 300 "
                                   "} strides { at_ms: 600 ahead: 4 } } }")
                  .ok());
  EXPECT_EQ(nanommo::ParseSkills("skills { id: 1 name: \"Dash\" level: 1 "
                                 "effect { cast_ms: 900 strides { at_ms: 600 } "
                                 "strides { at_ms: 300 ahead: 4 } } }")
                .status()
                .message(),
            "the Dash's strides go back in time");
  EXPECT_EQ(nanommo::ParseSkills("skills { id: 1 name: \"Dash\" level: 1 "
                                 "effect { cast_ms: 900 strides { at_ms: 300 } "
                                 "strides { at_ms: 1200 ahead: 4 } } }")
                .status()
                .message(),
            "the Dash takes a stride after it's over");
  EXPECT_EQ(nanommo::ParseSkills("starting: 2 skills { id: 1 name: \"Kick\" "
                                 "level: 1 effect { cast_ms: 900 } }")
                .status()
                .message(),
            "there's no skill 2 to start with");
}

TEST(Content, RefusesMovesThatDontAddUp) {
  EXPECT_TRUE(nanommo::ParseSkills("skills { id: 1 name: \"Kick\" level: 1 "
                                   "effect { cast_ms: 900 hit_ms: 300 } "
                                   "look { move { keys { at_ms: 200 } "
                                   "keys { at_ms: 900 } } } }")
                  .ok());
  EXPECT_EQ(nanommo::ParseSkills("skills { id: 1 name: \"Kick\" level: 1 "
                                 "effect { cast_ms: 900 hit_ms: 300 } "
                                 "look { move { keys { at_ms: 500 } "
                                 "keys { at_ms: 200 } } } }")
                .status()
                .message(),
            "the Kick's move goes back in time");
  EXPECT_EQ(nanommo::ParseSkills("skills { id: 1 name: \"Kick\" level: 1 "
                                 "effect { cast_ms: 900 hit_ms: 300 } "
                                 "look { move { keys { at_ms: 200 } "
                                 "keys { at_ms: 1000 } } } }")
                .status()
                .message(),
            "the Kick's move runs on after it's over");
}

TEST(Content, GivesEveryoneTheSwing) {
  const auto skills = nanommo::ParseSkills(
      "attack: 2 skills { id: 2 name: \"Strike\" level: 1 "
      "effect { weapon: true } }");
  ASSERT_TRUE(skills.ok());
  EXPECT_EQ(skills->attack, 2u);
  EXPECT_TRUE(skills->starting.contains(2));
  EXPECT_EQ(nanommo::ParseSkills("attack: 3 skills { id: 2 name: \"Strike\" "
                                 "level: 1 effect { weapon: true } }")
                .status()
                .message(),
            "there's no skill 3 to swing with");
}
