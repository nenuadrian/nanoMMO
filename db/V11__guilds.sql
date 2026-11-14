-- Guilds, each in one world, like its characters. A name is a world's once,
-- whatever its case, as MySQL's default collation compares without it.
CREATE TABLE guilds (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  world VARCHAR(16) NOT NULL,
  name VARCHAR(24) NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY guilds_world_name (world, name)
);

-- Who's in which guild, by the character's id, which is the primary key, so
-- a character is in one guild at most. Rank is a word MySQL keeps for itself,
-- so a member's rank is its grade: 0 a member, 1 an officer, 2 the leader.
-- A deleted character leaves its guild, and a guild that ends takes its
-- members' rows with it.
CREATE TABLE guild_members (
  player INT UNSIGNED NOT NULL PRIMARY KEY,
  guild INT UNSIGNED NOT NULL,
  grade TINYINT UNSIGNED NOT NULL DEFAULT 0,
  joined_at TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  KEY guild_members_guild (guild, grade),
  CONSTRAINT guild_members_player FOREIGN KEY (player)
    REFERENCES players (id) ON DELETE CASCADE,
  CONSTRAINT guild_members_guild FOREIGN KEY (guild)
    REFERENCES guilds (id) ON DELETE CASCADE
);
