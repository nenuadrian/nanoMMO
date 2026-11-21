-- Each kill in a war, a row each, as the saver takes it from Redis, in the
-- same transaction as its batch. Its id is the game server's, so a batch
-- that's tried again inserts it once, as a trade's is. A war's score is how
-- many of its rows each side has, by the killer's guild, which the key counts
-- without reading the rows. A war's kills go with it.
--
-- guild_wars' challenger_kills and challenged_kills aren't written any more.
-- They stay, as the servers of 0.91.0 read and write them until they're
-- replaced, and a migration keeps the release before it working.
CREATE TABLE war_kills (
  id CHAR(64) NOT NULL PRIMARY KEY,
  war INT UNSIGNED NOT NULL,
  world VARCHAR(16) NOT NULL,
  killer INT UNSIGNED NOT NULL,
  victim INT UNSIGNED NOT NULL,
  guild INT UNSIGNED NOT NULL,
  killed_at TIMESTAMP(3) NOT NULL,
  KEY war_kills_score (war, guild),
  CONSTRAINT war_kills_war FOREIGN KEY (war)
    REFERENCES guild_wars (id) ON DELETE CASCADE
);
