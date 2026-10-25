CREATE TABLE player_items (
  account_id INT UNSIGNED NOT NULL,
  world VARCHAR(16) NOT NULL,
  -- 'bag 0' to 'bag 23', or the slot it's worn in, such as 'main hand'.
  place VARCHAR(16) NOT NULL,
  item INT UNSIGNED NOT NULL,
  count SMALLINT UNSIGNED NOT NULL DEFAULT 1,
  -- What this one has of its own, such as its rolls, as protobuf's JSON for
  -- an items.Stack.
  state JSON NULL,
  PRIMARY KEY (account_id, world, place),
  FOREIGN KEY (account_id, world) REFERENCES players (account_id, world)
    ON DELETE CASCADE
);
