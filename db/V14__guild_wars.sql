-- Wars between two guilds of a world: the guild that asked, the one that said
-- yes, when it started and when it ends, half an hour on, and how many of the
-- other side's players each side has killed. A guild is in one war at a time,
-- which the game checks with both guilds' rows locked. An ended war stays on
-- record, and a guild that ends takes its wars with it.
CREATE TABLE guild_wars (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  world VARCHAR(16) NOT NULL,
  challenger INT UNSIGNED NOT NULL,
  challenged INT UNSIGNED NOT NULL,
  started_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  ends_at TIMESTAMP NOT NULL,
  challenger_kills INT UNSIGNED NOT NULL DEFAULT 0,
  challenged_kills INT UNSIGNED NOT NULL DEFAULT 0,
  KEY guild_wars_world (world, ends_at),
  CONSTRAINT guild_wars_challenger FOREIGN KEY (challenger)
    REFERENCES guilds (id) ON DELETE CASCADE,
  CONSTRAINT guild_wars_challenged FOREIGN KEY (challenged)
    REFERENCES guilds (id) ON DELETE CASCADE
);
