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

-- ---------------------------------------------------------------------
-- The Studio (BUILD studio 1, TheCommons/studio.md, reference pass TheCommons/studio-refs.md).
-- Corrections C1-C6 from the reference pass are applied directly below (there is no earlier shape to
-- reconcile -- every row here is new). Cut per studio.md: homemade vinegar from apples, alum crystals.
-- Not added: Prussian blue, lime, malachite. No recipe_inputs quantity is anything but 1: the game
-- deliberately tracks no stoichiometry (studio.md decision 2), only "do you hold this ingredient".
-- Distinct from every Lab-A item (vinegar_jar, oak_gall_tannin, lampblack_ink, alum_crystal,
-- gum_arabic, all 'placeable'/'wallable' trophies above): those stay untouched until slice 3's lab
-- conversion (studio.md's locks table) -- 'vinegar', 'tannin' and the rest below are the Studio's own,
-- category 'material', even where the real-world substance is the same one. oak_gall and seashell ARE
-- reused as-is: they were always gathered raw materials, never a finished trophy (World Pickups D).

INSERT OR IGNORE INTO items (slug, display_name, category, wall_slot_ok, sprite_ref, pigment_hex) VALUES
    -- raw materials (gathered; no pigment_hex, they are not paint)
    ('willow_twigs',    'Willow twigs',    'material', 0, 'studio_willow_twigs',  NULL),
    ('ochre_earth',     'Ochre earth',     'material', 0, 'studio_ochre_earth',   NULL),
    ('alum_stone',      'Alum stone',      'material', 0, 'studio_alum_stone',    NULL),
    ('madder_root',     'Madder root',     'material', 0, 'studio_madder_root',   NULL),
    ('weld',            'Weld',            'material', 0, 'studio_weld',          NULL),
    ('woad_leaves',     'Woad leaves',     'material', 0, 'studio_woad_leaves',   NULL),
    ('scrap_iron',      'Scrap iron',      'material', 0, 'studio_scrap_iron',    NULL),
    ('copper_sheet',    'Copper sheet',    'material', 0, 'studio_copper_sheet',  NULL),
    ('gum_arabic_raw',  'Gum arabic',      'material', 0, 'studio_gum_arabic_raw', NULL),
    ('soot',            'Soot',            'material', 0, 'studio_soot',          NULL),
    ('vinegar',         'Vinegar',         'material', 0, 'studio_vinegar',       NULL),
    -- pigments and intermediates (bench outputs). pigment_hex set only where the easel can paint with it.
    ('charcoal_black',       'Charcoal (black)',       'material', 0, 'studio_charcoal_black',       '#1a1a1a'),
    ('charcoal_grey_1',      'Charcoal (grey)',        'material', 0, 'studio_charcoal_grey_1',      '#4d4d4d'),
    ('charcoal_grey_2',      'Charcoal (light grey)',  'material', 0, 'studio_charcoal_grey_2',      '#7a7a7a'),
    ('charcoal_grey_3',      'Charcoal (pale grey)',   'material', 0, 'studio_charcoal_grey_3',      '#adadad'),
    ('shell_white',          'Shell white',            'material', 0, 'studio_shell_white',          '#f2ede4'),
    ('yellow_ochre',         'Yellow ochre',           'material', 0, 'studio_yellow_ochre',         '#c9963a'),
    ('red_ochre',            'Red ochre',              'material', 0, 'studio_red_ochre',            '#9c4a2e'),
    ('binder',               'Gum solution',           'material', 0, 'studio_binder',               NULL),
    ('tannin',               'Oak-gall tannin',        'material', 0, 'studio_tannin',               NULL),
    ('carbon_ink_black',     'Carbon ink',             'material', 0, 'studio_carbon_ink_black',     '#0d0d0d'),
    ('iron_acetate',         'Iron acetate',           'material', 0, 'studio_iron_acetate',         NULL),
    ('iron_gall_blue_black', 'Iron-gall ink',          'material', 0, 'studio_iron_gall_blue_black', '#1c1c24'),
    ('verdigris_green',      'Verdigris',              'material', 0, 'studio_verdigris_green',      '#3f8f5f'),
    ('madder_red',           'Madder lake',            'material', 0, 'studio_madder_red',           '#a72a2a'),
    ('weld_yellow',          'Weld lake',              'material', 0, 'studio_weld_yellow',          '#e0c04a'),
    ('woad_blue',            'Woad blue',              'material', 0, 'studio_woad_blue',            '#2e4c72'),
    -- BUILD property phase 2+3: a tool (held, never placed -- category 'material' like every other
    -- Studio intermediate) and a placeable structural item, both from materials that already existed.
    ('hammer',               'Hammer',                 'material',  0, 'studio_hammer',              NULL),
    ('fence_post',           'Fence post',              'placeable', 0, 'studio_fence_post',          NULL);

-- station is always 'bench' here, distinct from every Lab-A recipe's 'lab' -- studio.md: the bench (a
-- timed batch) and the lab (an instant Look) are different mechanics until slice 3 converts the lab.
INSERT OR IGNORE INTO recipes (slug, display_name, station, duration_s, tier, real_range_note, history_note) VALUES
    ('charcoal', 'Willow twigs -> charcoal', 'bench', 28800, 1,
     'Hours, plus cooling to handle (Cennini: left overnight).',
     'Left overnight, per Cennino Cennini''s manual (c. 1400); here: 8 hours.'),
    ('shell_white', 'Seashells -> shell white', 'bench', 86400, 2,
     'Historically weathered (months), then crushed.',
     'Ground shell, an old white pigment; not calcining (that is lime, which this game does not make).'),
    ('yellow_ochre', 'Ochre earth -> yellow ochre', 'bench', 86400, 3,
     'Unsourced; here: 1 day (washed and settled).',
     'Earth pigment, washed and settled; used since prehistory.'),
    ('red_ochre', 'Yellow ochre, heated -> red ochre', 'bench', 14400, 3,
     'Hours.',
     'Yellow ochre heated; here it is history, not a how-to.'),
    ('gum_solution', 'Gum arabic -> binder', 'bench', 28800, 3,
     'Overnight, to dissolve.',
     'Gum arabic dissolved in water; the traditional binder for ink and lake pigments.'),
    ('oak_gall_tannin_studio', 'Oak galls -> tannin', 'bench', 86400, 3,
     'A day or two, steeped.',
     'Oak galls steeped in water; the tannin base for iron-gall ink.'),
    ('carbon_ink', 'Soot + binder -> carbon ink', 'bench', 1800, 3,
     'Minutes to mix.',
     'Soot bound with gum arabic; the fast path -- the bench is for everything slower.'),
    ('iron_acetate', 'Scrap iron + vinegar -> iron acetate', 'bench', 259200, 4,
     'Unsourced; here: 3 days (folk method).',
     'Folk method: iron steeped in vinegar; period manuscripts more often used copperas (ferrous sulfate) instead.'),
    ('iron_gall_ink_studio', 'Tannin + iron acetate + binder -> iron-gall ink', 'bench', 43200, 4,
     'Darkens over hours.',
     'Tannin, iron and gum; ages from blue-black toward brown, as period manuscripts do.'),
    ('verdigris', 'Copper + vinegar -> verdigris', 'bench', 604800, 5,
     'Typically 1-3 weeks; here: 7 days (capped).',
     'Copper scraped after resting in vinegar fumes. Toxic; not for eating.'),
    ('madder_lake', 'Madder root + alum + shell white -> madder lake', 'bench', 172800, 6,
     'Days.',
     'Madder root precipitated onto shell white with alum; lake pigments like this are a later, northern-European technique.'),
    ('weld_lake', 'Weld + alum + shell white -> weld lake', 'bench', 86400, 6,
     'About a day.',
     'Weld precipitated onto shell white with alum, per a Bolognese-manuscript method.'),
    ('woad_blue', 'Woad + shell white -> woad blue', 'bench', 259200, 6,
     'Real vats ferment for weeks; here: 3 days, simplified.',
     'Woad froth settled onto shell white, a simplified vat -- no lye, no pH, no urine.'),
    -- BUILD property phase 2+3 (CLAUDE_PROPERTY_PHASE2.md): tier 1, same as charcoal -- a direct product
    -- of raw gathers, no intermediate dependency. Both reuse willow_twigs/scrap_iron; no new gather spot.
    ('hammer', 'Willow haft + scrap iron -> hammer', 'bench', 3600, 1,
     'Unsourced; here: 1 hour (a simple haft-and-head job).',
     'A hammer head set into a willow haft -- ordinary household ironwork, not a forge piece.'),
    ('fence_post', 'Willow twigs -> fence post', 'bench', 1800, 1,
     'Minutes to shape; here: 30 minutes.',
     'Willow, trimmed and pointed -- the plainest post there is, now or in any century.');

-- Inputs. Every quantity is 1 (decision 2: no stoichiometry). Water is implicit and untracked
-- (studio.md), so it is never listed here even where the real process uses it. C1: no ash anywhere
-- (dropped from both the charcoal by-product and the woad blue ingredient list).
INSERT OR IGNORE INTO recipe_inputs (recipe_id, item_id, quantity)
    SELECT r.recipe_id, i.item_id, 1
    FROM (SELECT 'charcoal' AS recipe, 'willow_twigs' AS item
          UNION ALL SELECT 'shell_white',             'seashell'
          UNION ALL SELECT 'yellow_ochre',            'ochre_earth'
          UNION ALL SELECT 'red_ochre',               'yellow_ochre'
          UNION ALL SELECT 'gum_solution',            'gum_arabic_raw'
          UNION ALL SELECT 'oak_gall_tannin_studio',  'oak_gall'
          UNION ALL SELECT 'carbon_ink',              'soot'
          UNION ALL SELECT 'carbon_ink',              'binder'
          UNION ALL SELECT 'iron_acetate',            'scrap_iron'
          UNION ALL SELECT 'iron_acetate',            'vinegar'
          UNION ALL SELECT 'iron_gall_ink_studio',    'tannin'
          UNION ALL SELECT 'iron_gall_ink_studio',    'iron_acetate'
          UNION ALL SELECT 'iron_gall_ink_studio',    'binder'
          UNION ALL SELECT 'verdigris',               'copper_sheet'
          UNION ALL SELECT 'verdigris',               'vinegar'
          UNION ALL SELECT 'madder_lake',             'madder_root'
          UNION ALL SELECT 'madder_lake',             'alum_stone'
          UNION ALL SELECT 'madder_lake',             'shell_white'
          UNION ALL SELECT 'weld_lake',               'weld'
          UNION ALL SELECT 'weld_lake',               'alum_stone'
          UNION ALL SELECT 'weld_lake',               'shell_white'
          UNION ALL SELECT 'woad_blue',               'woad_leaves'
          UNION ALL SELECT 'woad_blue',               'shell_white'
          UNION ALL SELECT 'hammer',                  'willow_twigs'
          UNION ALL SELECT 'hammer',                  'scrap_iron'
          UNION ALL SELECT 'fence_post',               'willow_twigs') m
    JOIN recipes r ON r.slug = m.recipe
    JOIN items   i ON i.slug = m.item;

-- Outputs. Charcoal alone yields four (the black + three greys, C1: no ash by-product); everything
-- else yields exactly one.
INSERT OR IGNORE INTO recipe_outputs (recipe_id, item_id, quantity)
    SELECT r.recipe_id, i.item_id, 1
    FROM (SELECT 'charcoal' AS recipe, 'charcoal_black' AS item
          UNION ALL SELECT 'charcoal',                 'charcoal_grey_1'
          UNION ALL SELECT 'charcoal',                 'charcoal_grey_2'
          UNION ALL SELECT 'charcoal',                 'charcoal_grey_3'
          UNION ALL SELECT 'shell_white',               'shell_white'
          UNION ALL SELECT 'yellow_ochre',              'yellow_ochre'
          UNION ALL SELECT 'red_ochre',                 'red_ochre'
          UNION ALL SELECT 'gum_solution',               'binder'
          UNION ALL SELECT 'oak_gall_tannin_studio',    'tannin'
          UNION ALL SELECT 'carbon_ink',                 'carbon_ink_black'
          UNION ALL SELECT 'iron_acetate',               'iron_acetate'
          UNION ALL SELECT 'iron_gall_ink_studio',       'iron_gall_blue_black'
          UNION ALL SELECT 'verdigris',                  'verdigris_green'
          UNION ALL SELECT 'madder_lake',                'madder_red'
          UNION ALL SELECT 'weld_lake',                  'weld_yellow'
          UNION ALL SELECT 'woad_blue',                  'woad_blue'
          UNION ALL SELECT 'hammer',                      'hammer'
          UNION ALL SELECT 'fence_post',                  'fence_post') m
    JOIN recipes r ON r.slug = m.recipe
    JOIN items   i ON i.slug = m.item;
