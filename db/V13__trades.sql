-- Each trade that's done, as the saver takes it from Redis, in the same
-- transaction as both its players' bags. Its id is the game server's, so a
-- batch that's tried again inserts it once. What each side gave is protobuf's
-- JSON for a TradeSide, with each item's serial. A character that's deleted
-- leaves its trades on record.
CREATE TABLE trades (
  id CHAR(64) NOT NULL PRIMARY KEY,
  world VARCHAR(16) NOT NULL,
  player INT UNSIGNED NOT NULL,
  player_gave JSON NOT NULL,
  partner INT UNSIGNED NOT NULL,
  partner_gave JSON NOT NULL,
  traded_at TIMESTAMP(3) NOT NULL,
  KEY trades_player (player, traded_at),
  KEY trades_partner (partner, traded_at)
);

-- Each item's serial, from its state, which MySQL works out as it reads a row,
-- and an index of them, built while the saver goes on writing, so that it
-- counts the serials held twice without reading each row's JSON.
ALTER TABLE player_items
  ADD COLUMN serial BIGINT UNSIGNED
    AS (CAST(state->>'$.serial' AS UNSIGNED)) VIRTUAL,
  ALGORITHM = INSTANT;
ALTER TABLE player_items
  ADD KEY player_items_serial (serial),
  ALGORITHM = INPLACE, LOCK = NONE;
