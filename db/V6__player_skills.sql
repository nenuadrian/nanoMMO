CREATE TABLE player_skills (
  account_id INT UNSIGNED NOT NULL,
  world VARCHAR(16) NOT NULL,
  skill INT UNSIGNED NOT NULL,
  PRIMARY KEY (account_id, world, skill),
  FOREIGN KEY (account_id, world) REFERENCES players (account_id, world)
    ON DELETE CASCADE
);
