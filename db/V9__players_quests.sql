-- The quests a player is on, with how many they've killed for each, and the
-- ones they've done, as protobuf's JSON for a protocol.Quests. They're read
-- and written whole, with the player, so they go with the player's row.
ALTER TABLE players
  ADD COLUMN quests JSON NULL,
  ALGORITHM = INSTANT;
