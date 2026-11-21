-- How each character looks under what they wear, as their player chose it,
-- making them: places in the rig's builds and in the figures file's lists of
-- skins, hair colours and ways to wear the hair. They never change, so only
-- MySQL has them, and a world reads them as the character enters it.
--
-- Characters from before there were looks to choose have none, and keep the
-- looks their id picks, so nobody wakes up as someone else.

ALTER TABLE players
  ADD COLUMN build TINYINT UNSIGNED NULL,
  ADD COLUMN skin TINYINT UNSIGNED NULL,
  ADD COLUMN hair_color TINYINT UNSIGNED NULL,
  ADD COLUMN hair TINYINT UNSIGNED NULL,
  ALGORITHM = INSTANT;
