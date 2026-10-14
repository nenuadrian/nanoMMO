ALTER TABLE players
  ADD COLUMN world VARCHAR(16) NOT NULL DEFAULT 'Ember' AFTER account_id,
  DROP PRIMARY KEY,
  ADD PRIMARY KEY (account_id, world);
