-- The colour each character's clothes are dyed in, as their player chose it,
-- making them: a place in the players' colours, as the other looks are places
-- in their lists.
--
-- Characters made before there was a colour to choose have none, and keep the
-- one their id picks.

ALTER TABLE players
  ADD COLUMN shirt TINYINT UNSIGNED NULL,
  ALGORITHM = INSTANT;
