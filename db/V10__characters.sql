-- An account can have several characters in a world, each with an id and a
-- name of its own, which is what other players see. Their skills and their
-- bags go by the character's id.
--
-- The world servers save into Redis first, so before this runs, they're
-- stopped and the saver has emptied Redis into MySQL: Redis keeps players by
-- the id they played as, which for the characters outside Ember changes here.

ALTER TABLE player_skills DROP FOREIGN KEY player_skills_ibfk_1;
ALTER TABLE player_items DROP FOREIGN KEY player_items_ibfk_1;

ALTER TABLE players
  ADD COLUMN id INT UNSIGNED NOT NULL DEFAULT 0 FIRST,
  ADD COLUMN name VARCHAR(16) NULL AFTER world;

-- Ember's characters keep their account's id, which they played as and which
-- their looks come from. The other worlds' come after the largest account id,
-- so that none of the two meet.
UPDATE players SET id = account_id WHERE world = 'Ember';
UPDATE players JOIN (
    SELECT account_id, world,
      (SELECT COALESCE(MAX(id), 0) FROM accounts)
        + ROW_NUMBER() OVER (ORDER BY account_id, world) AS id
    FROM players WHERE world <> 'Ember'
  ) AS numbered USING (account_id, world)
  SET players.id = numbered.id;
-- Each is named after its account, as it was.
UPDATE players JOIN accounts ON accounts.id = players.account_id
  SET players.name = accounts.username;

ALTER TABLE players
  DROP PRIMARY KEY,
  MODIFY COLUMN id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  MODIFY COLUMN name VARCHAR(16) NOT NULL,
  ADD PRIMARY KEY (id),
  ADD UNIQUE KEY players_world_name (world, name),
  ADD KEY players_account (account_id, world);

ALTER TABLE player_skills ADD COLUMN player INT UNSIGNED NOT NULL DEFAULT 0 FIRST;
UPDATE player_skills JOIN players USING (account_id, world)
  SET player_skills.player = players.id;
ALTER TABLE player_skills
  DROP PRIMARY KEY,
  DROP COLUMN account_id,
  DROP COLUMN world,
  ALTER COLUMN player DROP DEFAULT,
  ADD PRIMARY KEY (player, skill),
  ADD CONSTRAINT player_skills_player FOREIGN KEY (player)
    REFERENCES players (id) ON DELETE CASCADE;

ALTER TABLE player_items ADD COLUMN player INT UNSIGNED NOT NULL DEFAULT 0 FIRST;
UPDATE player_items JOIN players USING (account_id, world)
  SET player_items.player = players.id;
ALTER TABLE player_items
  DROP PRIMARY KEY,
  DROP COLUMN account_id,
  DROP COLUMN world,
  ALTER COLUMN player DROP DEFAULT,
  ADD PRIMARY KEY (player, place),
  ADD CONSTRAINT player_items_player FOREIGN KEY (player)
    REFERENCES players (id) ON DELETE CASCADE;
