#include "server/guilds.h"

#include <cstdint>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "server/accounts.h"
#include "server/characters.h"
#include "server/test_services.h"

TEST(GuildRules, AcceptsGoodNamesOnly) {
  EXPECT_TRUE(nanommo::CheckGuildName("Iron Wolves").ok());
  EXPECT_TRUE(nanommo::CheckGuildName("Ash").ok());
  EXPECT_FALSE(nanommo::CheckGuildName("Io").ok());
  EXPECT_FALSE(nanommo::CheckGuildName("The Wolves of the Iron Hill").ok());
  EXPECT_FALSE(nanommo::CheckGuildName("Iron  Wolves").ok());
  EXPECT_FALSE(nanommo::CheckGuildName(" Iron Wolves").ok());
  EXPECT_FALSE(nanommo::CheckGuildName("Iron Wolves ").ok());
  EXPECT_FALSE(nanommo::CheckGuildName("Wolves!").ok());
  EXPECT_FALSE(nanommo::CheckGuildName("Clan 7").ok());
}

namespace {

// Three characters of one account in a world of the test's own, so the
// guilds' names are free, and one in another world.
class GuildsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    database_ = nanommo::testing::TestDatabase();
    redis_ = nanommo::testing::TestRedis();
    if (!database_ || !redis_) {
      GTEST_SKIP() << "set NANOMMO_MYSQL_HOST and NANOMMO_REDIS_HOST to test "
                      "against MySQL and Redis";
    }
    const absl::StatusOr<uint32_t> account =
        nanommo::Accounts(*database_).Create(name_, "hash");
    ASSERT_TRUE(account.ok()) << account.status();
    nanommo::Characters characters(*database_);
    const absl::StatusOr<uint32_t> alice =
        characters.Create(*account, world_, name_);
    const absl::StatusOr<uint32_t> bob =
        characters.Create(*account, world_, name_ + "b");
    const absl::StatusOr<uint32_t> carol =
        characters.Create(*account, other_world_, name_);
    const absl::StatusOr<uint32_t> dave =
        characters.Create(*account, world_, name_ + "d");
    ASSERT_TRUE(alice.ok() && bob.ok() && carol.ok() && dave.ok());
    alice_ = *alice;
    bob_ = *bob;
    carol_ = *carol;
    dave_ = *dave;
  }

  // The accounts take their characters, and those their memberships, but a
  // guild stays until it's deleted.
  void TearDown() override {
    if (redis_) {
      for (const uint32_t id : {alice_, bob_, carol_, dave_}) {
        EXPECT_TRUE(
            redis_->Command({"DEL", absl::StrCat("guild_invite:", id)}).ok());
      }
    }
    if (database_) {
      EXPECT_TRUE(database_
                      ->Run("DELETE FROM guilds WHERE world IN (?, ?)",
                            {world_, other_world_})
                      .ok());
      EXPECT_TRUE(
          database_->Run("DELETE FROM accounts WHERE username = ?", {name_})
              .ok());
    }
  }

  std::unique_ptr<nanommo::Database> database_;
  std::unique_ptr<nanommo::Redis> redis_;
  const std::string name_ = nanommo::testing::TestName();
  const std::string world_ = name_;
  const std::string other_world_ = name_ + "b";
  uint32_t alice_ = 0;
  uint32_t bob_ = 0;
  uint32_t carol_ = 0;
  uint32_t dave_ = 0;
};

}  // namespace

TEST_F(GuildsTest, FoundsAGuildItsFounderLeads) {
  nanommo::Guilds guilds(*database_, *redis_);
  EXPECT_TRUE(absl::IsNotFound(guilds.Of(alice_).status()));

  const absl::StatusOr<nanommo::Membership> founded =
      guilds.Create(world_, alice_, "Iron Wolves");
  ASSERT_TRUE(founded.ok()) << founded.status();
  EXPECT_EQ(founded->name, "Iron Wolves");
  EXPECT_EQ(founded->rank, nanommo::protocol::RANK_LEADER);

  const absl::StatusOr<nanommo::Membership> of = guilds.Of(alice_);
  ASSERT_TRUE(of.ok()) << of.status();
  EXPECT_EQ(of->guild, founded->guild);
  EXPECT_EQ(of->name, "Iron Wolves");
  EXPECT_EQ(of->rank, nanommo::protocol::RANK_LEADER);
}

TEST_F(GuildsTest, RefusesANameTakenInItsWorldWhateverItsCase) {
  nanommo::Guilds guilds(*database_, *redis_);
  ASSERT_TRUE(guilds.Create(world_, alice_, "Iron Wolves").ok());

  EXPECT_TRUE(absl::IsAlreadyExists(
      guilds.Create(world_, bob_, "IRON WOLVES").status()));
  EXPECT_TRUE(guilds.Create(other_world_, carol_, "Iron Wolves").ok());
}

TEST_F(GuildsTest, ACharacterFoundsOneGuildAtMost) {
  nanommo::Guilds guilds(*database_, *redis_);
  ASSERT_TRUE(guilds.Create(world_, alice_, "Iron Wolves").ok());

  EXPECT_TRUE(absl::IsAlreadyExists(
      guilds.Create(world_, alice_, "Stone Bears").status()));
  // The refused guild went with its leader, so its name is still free.
  EXPECT_TRUE(guilds.Create(world_, bob_, "Stone Bears").ok());
}

TEST_F(GuildsTest, AnInvitationIsOneAtATimeAndTakenOnce) {
  nanommo::Guilds guilds(*database_, *redis_);
  ASSERT_TRUE(guilds.Create(world_, alice_, "Iron Wolves").ok());

  ASSERT_TRUE(guilds.Invite(alice_, bob_).ok());
  EXPECT_TRUE(absl::IsAlreadyExists(guilds.Invite(alice_, bob_).status()));
  const absl::StatusOr<nanommo::Membership> joined = guilds.Accept(bob_);
  ASSERT_TRUE(joined.ok()) << joined.status();
  EXPECT_EQ(joined->name, "Iron Wolves");
  EXPECT_EQ(joined->rank, nanommo::protocol::RANK_MEMBER);
  EXPECT_TRUE(absl::IsNotFound(guilds.Accept(bob_).status()));

  const absl::StatusOr<nanommo::Membership> of = guilds.Of(bob_);
  ASSERT_TRUE(of.ok());
  EXPECT_EQ(of->guild, joined->guild);
}

TEST_F(GuildsTest, MembersCantAskSomeoneIn) {
  nanommo::Guilds guilds(*database_, *redis_);
  ASSERT_TRUE(guilds.Create(world_, alice_, "Iron Wolves").ok());
  ASSERT_TRUE(guilds.Invite(alice_, bob_).ok());
  ASSERT_TRUE(guilds.Accept(bob_).ok());

  EXPECT_TRUE(absl::IsPermissionDenied(guilds.Invite(bob_, dave_).status()));
  EXPECT_TRUE(absl::IsNotFound(guilds.Invite(dave_, bob_).status()));
}

TEST_F(GuildsTest, ACharacterInAGuildCantJoinAnother) {
  nanommo::Guilds guilds(*database_, *redis_);
  ASSERT_TRUE(guilds.Create(world_, alice_, "Iron Wolves").ok());
  ASSERT_TRUE(guilds.Create(world_, bob_, "Stone Bears").ok());
  ASSERT_TRUE(guilds.Invite(alice_, bob_).ok());

  EXPECT_TRUE(absl::IsAlreadyExists(guilds.Accept(bob_).status()));
  EXPECT_EQ(guilds.Of(bob_)->name, "Stone Bears");
}

TEST_F(GuildsTest, SayingNoLetsAnotherGuildAsk) {
  nanommo::Guilds guilds(*database_, *redis_);
  ASSERT_TRUE(guilds.Create(world_, alice_, "Iron Wolves").ok());
  ASSERT_TRUE(guilds.Create(world_, dave_, "Stone Bears").ok());
  ASSERT_TRUE(guilds.Invite(alice_, bob_).ok());
  EXPECT_TRUE(absl::IsAlreadyExists(guilds.Invite(dave_, bob_).status()));

  ASSERT_TRUE(guilds.Decline(bob_).ok());
  ASSERT_TRUE(guilds.Invite(dave_, bob_).ok());
  const absl::StatusOr<nanommo::Membership> joined = guilds.Accept(bob_);
  ASSERT_TRUE(joined.ok()) << joined.status();
  EXPECT_EQ(joined->name, "Stone Bears");
}
