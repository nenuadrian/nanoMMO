-- A player's gold, which is in their bag's message, with the bag's revision,
-- so the saver writes it as it does the bag, when that's changed.
ALTER TABLE players
  ADD COLUMN gold INT UNSIGNED NULL,
  ALGORITHM = INSTANT;
