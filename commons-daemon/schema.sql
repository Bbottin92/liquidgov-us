-- Runtime copy, executed by the daemon at startup. The spec source of
-- truth is Persistence/TheCommons/schema.sql -- keep this file identical
-- to it; this duplication exists only because the daemon needs a real
-- file to read at its own path, not because the schema is decided here.
--
-- The Commons — durable schema (sibling game, not LGP1, not liquidgov_http)
--
-- ~3NF catalogs, ~2NF instances. Design target is the eventual droplet
-- sibling process; the Grok preview may seed/mock this in memory for V1
-- (see v1.md) but should not invent a different shape when it does wire
-- real persistence later.
--
-- No friend graph table here, ever — friends are a read-only query against
-- LiquidGov (see glue.md). No LGP1 tags. Any multi-row mutation (a trade,
-- a place+unplace pair) must be BEGIN/COMMIT: LGP1 is fail-closed and
-- cannot roll back a half-write, and this store must not inherit that.

PRAGMA foreign_keys = ON;

-- ---------------------------------------------------------------------
-- Catalogs
-- ---------------------------------------------------------------------

CREATE TABLE items (
    item_id      INTEGER PRIMARY KEY,
    slug         TEXT NOT NULL UNIQUE,        -- e.g. 'copper_acetate_bottle'
    display_name TEXT NOT NULL,
    category     TEXT NOT NULL CHECK (category IN ('placeable','wallable','material')),
    wall_slot_ok INTEGER NOT NULL DEFAULT 0,  -- 1 if it may occupy the one profile-canvas wall slot
    sprite_ref   TEXT NOT NULL                -- key into art.md's V1 drawable set, not a blob
);

CREATE TABLE recipes (
    recipe_id    INTEGER PRIMARY KEY,
    slug         TEXT NOT NULL UNIQUE,        -- e.g. 'copper_acetate'
    display_name TEXT NOT NULL,
    station      TEXT NOT NULL DEFAULT 'lab'  -- future-proofing only; V1 has exactly one station
);

CREATE TABLE recipe_inputs (
    recipe_id    INTEGER NOT NULL REFERENCES recipes(recipe_id),
    item_id      INTEGER NOT NULL REFERENCES items(item_id),
    quantity     INTEGER NOT NULL DEFAULT 1,
    PRIMARY KEY (recipe_id, item_id)
);
-- V1 note: the three lab lookups are real-name flavor, not a crafting
-- economy (see pushback.md) -- recipe_inputs may legitimately have zero
-- rows for a V1 recipe. The table exists so a later real crafting loop is
-- additive, not a migration.

CREATE TABLE recipe_outputs (
    recipe_id    INTEGER NOT NULL REFERENCES recipes(recipe_id),
    item_id      INTEGER NOT NULL REFERENCES items(item_id),
    quantity     INTEGER NOT NULL DEFAULT 1,
    PRIMARY KEY (recipe_id, item_id)
);

CREATE TABLE maps (
    map_id       INTEGER PRIMARY KEY,
    slug         TEXT NOT NULL UNIQUE,        -- 'north_woods', 'the_square', 'lab_interior', 'house', ...
    display_name TEXT NOT NULL,
    kind         TEXT NOT NULL CHECK (kind IN ('outdoor','interior')),
    width_tiles  INTEGER NOT NULL,
    height_tiles INTEGER NOT NULL,
    -- Outdoor regions only: position in the 3x4 supermap (see atlas.md),
    -- used for the continuous-world + presence AABB math in presence.md.
    -- NULL for interior template maps.
    grid_col     INTEGER,
    grid_row     INTEGER
);
-- The 12 outdoor regions are exactly 12 rows here with kind='outdoor' and a
-- grid_col/grid_row set. There is exactly one live copy of each, so they
-- never need a map_instances row. Interior *templates* ('house', 'lab') are
-- rows here too, with kind='interior' and grid_col/grid_row NULL; actual
-- playable copies of an interior are map_instances rows below.

-- One row per instantiable interior copy (a specific player's house, the
-- one shared demo house, the lab -- even though the lab has only one
-- instance in V1, it still goes through this table since it's an interior).
-- owner_player_id is declared without a hard FK here (players is defined
-- next, and SQLite resolves FK targets at DML time not CREATE time either
-- way) to avoid a forward reference; the relationship is still enforced by
-- the app layer / PRAGMA foreign_keys once both tables exist.
CREATE TABLE map_instances (
    instance_id      INTEGER PRIMARY KEY,
    map_id           INTEGER NOT NULL REFERENCES maps(map_id),
    owner_player_id  INTEGER,                  -- REFERENCES players(player_id); NULL for demo house / lab
    created_at       INTEGER NOT NULL
);

CREATE TABLE players (
    player_id         INTEGER PRIMARY KEY,
    -- Reference only, never a second login/password. NULL until a guest
    -- binds a LiquidGov session cookie (see glue.md). Guests still need a
    -- row so presence/placed/inventory rows have something to point at
    -- during their session.
    lg_username        TEXT UNIQUE,
    display_name       TEXT NOT NULL,          -- throwaway name pre-bind, lg_username-derived after
    is_guest           INTEGER NOT NULL DEFAULT 1,
    house_instance_id  INTEGER REFERENCES map_instances(instance_id), -- NULL until bound
    created_at         INTEGER NOT NULL,       -- unix seconds
    -- Leave / ~30s snapshot for bound players (see presence.md). Outdoor:
    -- last_grid_col/row + last_x/y local to that region. Interior:
    -- last_instance_id + last_x/y. Exactly one of (grid pair) or instance
    -- is set. All nullable until the first snapshot. Not used in V1.
    last_grid_col      INTEGER,
    last_grid_row      INTEGER,
    last_instance_id   INTEGER REFERENCES map_instances(instance_id),
    last_x             INTEGER,
    last_y             INTEGER,
    last_facing        INTEGER
);

CREATE TABLE inventory (
    player_id    INTEGER NOT NULL REFERENCES players(player_id),
    item_id      INTEGER NOT NULL REFERENCES items(item_id),
    quantity     INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (player_id, item_id)
);

-- Source of truth for anything placed in an interior instance (furniture,
-- the one wall-slot occupant). A house-snapshot JSON blob, if the client
-- wants one for fast load, is a CACHE derived from this table on read --
-- never edited directly, never the thing a write targets.
CREATE TABLE placed (
    placed_id     INTEGER PRIMARY KEY,
    instance_id   INTEGER NOT NULL REFERENCES map_instances(instance_id),
    item_id       INTEGER NOT NULL REFERENCES items(item_id),
    x             INTEGER NOT NULL,
    y             INTEGER NOT NULL,
    rot90         INTEGER NOT NULL DEFAULT 0 CHECK (rot90 IN (0,1,2,3)),
    is_wall_slot  INTEGER NOT NULL DEFAULT 0  -- 1 for the single profile-canvas slot in a house
);

-- Exactly one wall-slot occupant per interior instance.
CREATE UNIQUE INDEX placed_wall_slot_unique
    ON placed(instance_id) WHERE is_wall_slot = 1;

-- ---------------------------------------------------------------------
-- Reserved, not created in V1 (see v1.md) -- no trading loop exists yet.
-- Shape is noted here only so adding it later doesn't require touching the
-- tables above:
--
-- CREATE TABLE trades (
--     trade_id     INTEGER PRIMARY KEY,
--     initiator_id INTEGER NOT NULL REFERENCES players(player_id),
--     counterparty_id INTEGER NOT NULL REFERENCES players(player_id),
--     status       TEXT NOT NULL CHECK (status IN ('pending','committed','cancelled')),
--     created_at   INTEGER NOT NULL
-- );
-- Every trade mutation is BEGIN/COMMIT across trades + inventory rows.
-- ---------------------------------------------------------------------

-- Presence (position, who's in the plaza) is explicitly NOT a table here.
-- It is RAM-only per presence.md, with a periodic snapshot -- if that
-- snapshot is ever persisted for crash recovery, bound last_* columns
-- above are the place; not a normalized presence table.
