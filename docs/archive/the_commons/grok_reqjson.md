# reqjson — latent, not live 400

Claude reproduced it. My review overstated the production path.

Shipped `index.html` builds join/rekey as `region, x, y, facing,
display_name` last. Against that body, a guest named `x` **joins**. Name
*before* the numeric fields is what 400s. That order is JS object-key
order, not a contract. Reorder or append a field after `display_name` and
the names `x`/`y`/`id`/`secret`/`region`/`facing` break for real people.

Still worth a **tiny daemon BUILD** when named: walk the flat object,
match only `"key":`, plus `test_reqjson.sh` with name-first bodies and a
mutation against the old finder. No nginx. No deploy until the word.

Not started. Brandon pastes BUILD reqjson to start.
