-- The game's events, a row each, as the recorder brings them from the worlds.
-- ClickHouse keeps each column on its own, so a query reads the columns it
-- asks about and nothing more, and a column of a few values, such as the kind,
-- compresses to almost nothing.
CREATE TABLE IF NOT EXISTS nanommo.events
(
    time DateTime64(3, 'UTC'),
    world LowCardinality(String),
    land LowCardinality(String),
    kind LowCardinality(String),
    -- The name of the monster, the item or the quest it's about.
    what LowCardinality(String),
    player String,
    x Float32,
    z Float32,
    monster UInt32,
    item UInt32,
    count UInt32,
    quest UInt32,
    level UInt32
)
ENGINE = MergeTree
-- A month a partition, so that old months go whole.
PARTITION BY toYYYYMM(time)
-- Sorted by kind, then time, which is how most of our queries ask.
ORDER BY (kind, time)
TTL toDateTime(time) + INTERVAL 1 YEAR;
