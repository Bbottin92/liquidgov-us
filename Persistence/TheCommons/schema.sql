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
    sprite_ref   TEXT NOT NULL,               -- key into art.md's V1 drawable set, not a blob
    -- The Studio (BUILD studio 1): NULL for a raw material or an intermediate
    -- (tannin, binder, iron acetate -- not directly paintable); set only for
    -- something the easel can actually put on the canvas. Existing DBs get
    -- this from Database::ensure_studio_columns().
    pigment_hex  TEXT
);

CREATE TABLE recipes (
    recipe_id       INTEGER PRIMARY KEY,
    slug            TEXT NOT NULL UNIQUE,     -- e.g. 'copper_acetate'
    display_name    TEXT NOT NULL,
    station         TEXT NOT NULL DEFAULT 'lab', -- future-proofing only; V1 has exactly one station
    -- The Studio (BUILD studio 1): NULL on a Lab-A lookup (not a benchable
    -- batch); set together on a Studio recipe. duration_s is the server-clock
    -- batch time; tier is the ladder position; the two notes are what the
    -- notebook will show once a client reads them -- a real time range (or
    -- NULL/"here: Ns" where the reference pass found no source) and a
    -- one-line history, never a how-to (studio.md's safety line). Existing
    -- DBs get these four from Database::ensure_studio_columns().
    duration_s      INTEGER,
    tier            INTEGER,
    real_range_note TEXT,
    history_note    TEXT
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
    last_facing        INTEGER,
    -- Who may visit this player's own house (house policy): 'friends' = mutual
    -- non-blocked friends while the owner is in the game (the visiting rule);
    -- 'locked' = nobody but the owner; 'open' (BUILD open A) = any bound player
    -- who is not blocked either way (civic's /internal/blocked answers that,
    -- failing closed) while the owner is in the game. Any other value is read as
    -- 'friends'. Only ever set by the owner.
    visit_policy       TEXT NOT NULL DEFAULT 'friends'
);

CREATE TABLE inventory (
    player_id    INTEGER NOT NULL REFERENCES players(player_id),
    item_id      INTEGER NOT NULL REFERENCES items(item_id),
    quantity     INTEGER NOT NULL DEFAULT 0,
    -- Unix *milliseconds* of the last pickup of this item, so "the item I
    -- most recently picked up" (what a wall slot places) survives a
    -- restart -- ms, not seconds, so two pickups in one second still order.
    -- Added after the table first shipped: an existing DB gets it from
    -- Database::ensure_inventory_recency() at startup, since migrate() only
    -- runs this file when the DB is brand new. DEFAULT 0 = "before recency
    -- was tracked".
    updated_at   INTEGER NOT NULL DEFAULT 0,
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

-- ---------------------------------------------------------------------
-- The Studio (BUILD studio 1, TheCommons/studio.md) -- gather real materials,
-- make pigments at a bench on a server-clock timer, paint with what you have
-- earned. Slice 1 is this schema + the daemon API; no client, no droplet
-- until `deploy`. Existing DBs get the two new columns and four new tables
-- from Database::ensure_studio_columns() / ensure_studio_tables() at
-- startup (migrate() only runs this file on a brand-new DB).
-- ---------------------------------------------------------------------

-- items.pigment_hex and the four new recipes columns are declared above,
-- on the CREATE TABLE statements themselves (a fresh database gets them
-- immediately); Database::ensure_studio_columns() ALTERs them onto an
-- EXISTING database that predates this build, same shape as
-- ensure_inventory_recency() / ensure_visit_policy() before it. category
-- stays 'material' for every Studio item -- the CHECK above cannot be
-- widened in SQLite, and nothing about "is this a pigment" needs a new
-- category to be true.

-- A started bench batch. started_ms/ready_ms are the server clock ONLY --
-- the client is never trusted with either (studio.md's data model). A
-- batch finishes even while the player is away; collect is a pull, not a
-- push. Slot cap (3 uncollected at once per player) is enforced in code,
-- not here, since SQLite has no per-group row-count CHECK.
CREATE TABLE batches (
    batch_id     INTEGER PRIMARY KEY,
    player_id    INTEGER NOT NULL REFERENCES players(player_id),
    recipe_id    INTEGER NOT NULL REFERENCES recipes(recipe_id),
    started_ms   INTEGER NOT NULL,
    ready_ms     INTEGER NOT NULL,
    collected    INTEGER NOT NULL DEFAULT 0
);

-- One row the instant a recipe is first ever collected by a player --
-- existence IS the discovery; the PK makes the first INSERT OR IGNORE
-- idempotent and second-and-later collects of the same recipe a no-op here.
CREATE TABLE discoveries (
    player_id    INTEGER NOT NULL REFERENCES players(player_id),
    recipe_id    INTEGER NOT NULL REFERENCES recipes(recipe_id),
    first_ms     INTEGER NOT NULL,
    PRIMARY KEY (player_id, recipe_id)
);

-- Per-player, per-spot gather cooldown. `spot` is a fixed slug from the
-- server's own static table (studio.cpp), not client-authored text -- a
-- region gathered from today, e.g. 'north_woods_galls', 'orchard_keg'.
-- The UNIQUE constraint is what makes the gather endpoint's single
-- INSERT ... ON CONFLICT DO UPDATE ... WHERE race-safe (two racing POSTs
-- for the same player+spot cannot both pass; see StudioStore::gather).
CREATE TABLE gather_cooldowns (
    player_id    INTEGER NOT NULL REFERENCES players(player_id),
    spot         TEXT NOT NULL,
    next_ms      INTEGER NOT NULL,
    UNIQUE(player_id, spot)
);

-- Slice 2 uses this (the studio portrait); the table exists from slice 1
-- so the orphan reap already knows it -- civic account-deletion must not
-- be able to leak a painting the way house rows once could (studio.md).
-- pixels is a palette-index bitmap (1 byte/pixel), never a PNG.
CREATE TABLE paintings (
    painting_id  INTEGER PRIMARY KEY,
    player_id    INTEGER NOT NULL REFERENCES players(player_id),
    kind         TEXT NOT NULL CHECK (kind IN ('portrait','painting')),
    w            INTEGER NOT NULL,
    h            INTEGER NOT NULL,
    pixels       BLOB NOT NULL,
    updated_ms   INTEGER NOT NULL,
    UNIQUE(player_id, kind)
);
-- ---------------------------------------------------------------------

-- Presence (position, who's in the plaza) is explicitly NOT a table here.
-- It is RAM-only per presence.md, with a periodic snapshot -- if that
-- snapshot is ever persisted for crash recovery, bound last_* columns
-- above are the place; not a normalized presence table.
