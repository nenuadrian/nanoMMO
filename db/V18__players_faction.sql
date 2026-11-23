-- The faction each character has sworn to, by its id in the factions file, or
-- NULL for none. It changes in the world, as a player swears to one at its
-- herald, so the world saves it into Redis with the rest of the player, and
-- the saver writes it here, only ever over a NULL: a faction is for good.

ALTER TABLE players
  ADD COLUMN faction TINYINT UNSIGNED NULL,
  ALGORITHM = INSTANT;
