-- Idempotent catalog growth. migrate() only runs seed.sql when `items` is
-- empty, so a live database that already has the original three lookups
-- never sees new rows from seed.sql. This file is exec'd on every daemon
-- start (Database::ensure_catalog) with INSERT OR IGNORE, so fresh DBs
-- (seed then this) and production (this only) converge.
--
-- Five more lab lookups (GROK_OBJECTS_ANSWER.md). No lime: chalk-only is
-- locked (pushback.md). No recipe_inputs: still real-name lookups, not a
-- crafting economy.
--
-- Three world pickups (GROK_OBJECTS_ANSWER.md D). Same loop, never
-- depleted, per player. No limestone: Quarry Ridge sits next to
-- "No ore. That's honest." No recipes: these are not lab stations.

INSERT OR IGNORE INTO items (slug, display_name, category, wall_slot_ok, sprite_ref) VALUES
    ('vinegar_jar',     'Vinegar jar',     'placeable', 0, 'lab_bottle'),
    ('oak_gall_tannin', 'Oak-gall tannin', 'placeable', 0, 'lab_jar'),
    ('lampblack_ink',   'Lampblack ink',   'wallable',  0, 'lab_page'),
    ('alum_crystal',    'Alum crystal',    'placeable', 0, 'lab_crystal'),
    ('gum_arabic',      'Gum arabic',      'placeable', 0, 'lab_jar'),
    ('oak_gall',        'Oak galls',       'material',  0, 'world_galls'),
    ('seashell',        'Seashells',       'material',  0, 'world_shells'),
    ('cider_apple',     'Cider apples',    'material',  0, 'world_apples');

INSERT OR IGNORE INTO recipes (slug, display_name, station) VALUES
    ('vinegar',        'Acetic acid from fermented cider',                    'lab'),
    ('oak_gall_tannin','Oak galls, crushed and steeped -> tannin',            'lab'),
    ('lampblack_ink',  'Soot + gum arabic -> carbon ink',                     'lab'),
    ('alum',           'Potassium aluminium sulfate, crystallised',           'lab'),
    ('gum_arabic',     'Acacia resin dissolved in water',                     'lab');

INSERT OR IGNORE INTO recipe_outputs (recipe_id, item_id, quantity)
    SELECT r.recipe_id, i.item_id, 1
    FROM recipes r JOIN items i ON i.slug = 'vinegar_jar'
    WHERE r.slug = 'vinegar';
INSERT OR IGNORE INTO recipe_outputs (recipe_id, item_id, quantity)
    SELECT r.recipe_id, i.item_id, 1
    FROM recipes r JOIN items i ON i.slug = 'oak_gall_tannin'
    WHERE r.slug = 'oak_gall_tannin';
INSERT OR IGNORE INTO recipe_outputs (recipe_id, item_id, quantity)
    SELECT r.recipe_id, i.item_id, 1
    FROM recipes r JOIN items i ON i.slug = 'lampblack_ink'
    WHERE r.slug = 'lampblack_ink';
INSERT OR IGNORE INTO recipe_outputs (recipe_id, item_id, quantity)
    SELECT r.recipe_id, i.item_id, 1
    FROM recipes r JOIN items i ON i.slug = 'alum_crystal'
    WHERE r.slug = 'alum';
INSERT OR IGNORE INTO recipe_outputs (recipe_id, item_id, quantity)
    SELECT r.recipe_id, i.item_id, 1
    FROM recipes r JOIN items i ON i.slug = 'gum_arabic'
    WHERE r.slug = 'gum_arabic';
