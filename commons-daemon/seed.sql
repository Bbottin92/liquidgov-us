-- Catalog seed data, run once (db.cpp only executes this when `items` is
-- still empty). Matches the locked spec exactly:
--   - Three lab lookups, pushback.md's chalk-only fix (no lime).
--   - Twelve outdoor regions + two interior templates, atlas.md's grid.
-- Not the profile-canvas wall slot itself (that's glue-era, fetched from
-- civic, never a local catalog row) -- but the *slot* an item can occupy
-- is a placed.is_wall_slot flag, not a special item, so nothing extra is
-- needed here for it.

INSERT INTO items (slug, display_name, category, wall_slot_ok, sprite_ref) VALUES
    ('copper_acetate_bottle', 'Copper acetate bottle', 'placeable', 0, 'lab_bottle'),
    ('iron_gall_ink_page',    'Iron-gall ink page',    'wallable',  0, 'lab_page'),
    ('chalk_block',           'Chalk block',           'placeable', 0, 'lab_chalk');

INSERT INTO recipes (slug, display_name, station) VALUES
    ('copper_acetate', 'Copper + acetic acid -> copper acetate', 'lab'),
    ('iron_gall_ink',  'Iron + tannin -> iron-gall ink',         'lab'),
    ('chalk',          'Calcium carbonate, ground and washed -> chalk', 'lab');

-- No recipe_inputs rows: these are real-name lookups, not a crafting
-- economy that consumes anything (pushback.md).
INSERT INTO recipe_outputs (recipe_id, item_id, quantity)
    SELECT r.recipe_id, i.item_id, 1
    FROM recipes r JOIN items i ON i.slug = 'copper_acetate_bottle'
    WHERE r.slug = 'copper_acetate';
INSERT INTO recipe_outputs (recipe_id, item_id, quantity)
    SELECT r.recipe_id, i.item_id, 1
    FROM recipes r JOIN items i ON i.slug = 'iron_gall_ink_page'
    WHERE r.slug = 'iron_gall_ink';
INSERT INTO recipe_outputs (recipe_id, item_id, quantity)
    SELECT r.recipe_id, i.item_id, 1
    FROM recipes r JOIN items i ON i.slug = 'chalk_block'
    WHERE r.slug = 'chalk';

-- Twelve outdoor regions, atlas.md's exact grid (col,row), 24x20 each.
INSERT INTO maps (slug, display_name, kind, width_tiles, height_tiles, grid_col, grid_row) VALUES
    ('north_woods',   'North Woods',   'outdoor', 24, 20, 0, 0),
    ('lookout',       'Lookout',       'outdoor', 24, 20, 1, 0),
    ('orchard',       'Orchard',       'outdoor', 24, 20, 2, 0),
    ('workshop_lane', 'Workshop Lane', 'outdoor', 24, 20, 0, 1),
    ('the_square',    'the Square',    'outdoor', 24, 20, 1, 1),
    ('hall_lawn',     'Hall Lawn',     'outdoor', 24, 20, 2, 1),
    ('lab_yard',      'Lab Yard',      'outdoor', 24, 20, 0, 2),
    ('homestead_row', 'Homestead Row', 'outdoor', 24, 20, 1, 2),
    ('garden',        'Garden',        'outdoor', 24, 20, 2, 2),
    ('quarry_ridge',  'Quarry Ridge',  'outdoor', 24, 20, 0, 3),
    ('creek',         'Creek',         'outdoor', 24, 20, 1, 3),
    ('shore',         'Shore',         'outdoor', 24, 20, 2, 3);

-- Interior templates. grid_col/row NULL -- these aren't on the supermap.
INSERT INTO maps (slug, display_name, kind, width_tiles, height_tiles, grid_col, grid_row) VALUES
    ('house', 'House', 'interior', 16, 16, NULL, NULL),
    ('lab',   'Lab',   'interior', 20, 16, NULL, NULL);

-- The one shared demo house instance, owner NULL (v1.md: only reachable
-- interior instance in V1; per-player houses come with real glue).
INSERT INTO map_instances (map_id, owner_player_id, created_at)
    SELECT map_id, NULL, strftime('%s','now') FROM maps WHERE slug = 'house';

-- The lab instance -- one shared copy, same reasoning as the demo house.
INSERT INTO map_instances (map_id, owner_player_id, created_at)
    SELECT map_id, NULL, strftime('%s','now') FROM maps WHERE slug = 'lab';
