#pragma once
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "sqlite3.h"

// The Studio (BUILD studio 1, TheCommons/studio.md): gather, bench, mix, collect -- the daemon-only
// slice. No client reads any of this yet. Shares the existing `inventory` table with HouseStore's lab
// pickups (one inventory per player, regardless of source); owns `batches`, `discoveries` and
// `gather_cooldowns` outright. `paintings` exists from this slice's schema but nothing here reads or
// writes it -- that is slice 2.
//
// No mutex, same reasoning as HouseStore: this daemon is single-threaded and synchronous.
namespace commons {

class StudioStore {
public:
    explicit StudioStore(sqlite3* db) : db_(db) {}

    // ---- gather (POST /presence/gather {spot}) ----------------------------------------------------
    struct GatherResult {
        bool ok = false;
        std::string reason;  // "" on ok; else "unknown_spot" / "on_cooldown"
        std::string item;    // the slug granted, if ok
        int qty = 0;
        long long next_ms = 0;  // when this spot is free again (set whether ok or on_cooldown)
    };
    // `spot` is looked up in the fixed static table below (studio.cpp) -- never client-authored data,
    // so an unknown spot is a caller bug, not a game event. Cooldown row + inventory grant happen in
    // one transaction (schema.sql: the UNIQUE(player_id, spot) is what makes this race-safe against a
    // second identical POST arriving before the first commits).
    GatherResult gather(const std::string& username, const std::string& spot, long long now_ms);

    // ---- bench (POST /presence/bench {recipe}) -----------------------------------------------------
    struct BenchResult {
        bool ok = false;
        std::string reason;  // "" on ok; else "unknown_recipe" / "slots_full" / "missing_inputs"
        int batch_id = 0;
        long long ready_ms = 0;
    };
    // Only a recipe with duration_s set (a Studio recipe, never a Lab-A lookup) can be benched. Slot
    // cap (3 uncollected batches at once per player), the input-quantity check, and the deduction are
    // all inside one transaction: a race that leaves too little of an input aborts the whole batch,
    // never a partial deduction (schema.sql: "gather, bench and collect are each one transaction").
    BenchResult start_batch(const std::string& username, const std::string& recipe_slug, long long now_ms);

    // ---- mix (POST /presence/mix {slugs}) -----------------------------------------------------------
    // Stateless and read-only: matches the sorted multiset of slugs (duplicates kept, order ignored)
    // against a small fixed table of real fail texts, or "Nothing useful comes of it." for anything
    // else. Never touches inventory, never returns a recipe_id -- it cannot start a batch or discover
    // anything (studio.md, discovery rule 1). The 1-per-5s rate limit lives in main.cpp (RAM, per
    // player), not here: it has nothing to do with the database.
    std::string mix(std::vector<std::string> slugs) const;

    // ---- collect (POST /presence/collect {batch_id}) -------------------------------------------------
    struct CollectResult {
        bool ok = false;
        std::string reason;  // "" on ok; else "not_found" / "not_yours" / "not_ready" / "already_collected"
        bool first_discovery = false;
        std::vector<std::pair<std::string, int>> outputs;  // (item slug, qty granted)
    };
    // `collected` flips 0 -> 1 exactly once (idempotent: a second collect of the same batch_id is
    // "already_collected", not a second grant). Authoritative on `ready_ms` read from the row itself
    // -- the client's own countdown is display only, never trusted (studio.md).
    CollectResult collect(const std::string& username, int batch_id, long long now_ms);

    // ---- GET /presence/studio, assembled piece by piece (main.cpp builds the JSON) -------------------
    struct BatchView { int batch_id; std::string recipe; long long ready_ms; };  // uncollected only
    std::vector<BatchView> open_batches(const std::string& username);
    struct DiscoveryView { std::string recipe; long long first_ms; };
    std::vector<DiscoveryView> discoveries_of(const std::string& username);
    struct CooldownView { std::string spot; long long next_ms; };
    std::vector<CooldownView> cooldowns_of(const std::string& username, long long now_ms);  // only ones still active
    struct RecipeView { std::string slug; std::string display_name; int duration_s; };
    std::vector<RecipeView> makeable_now(const std::string& username);  // every input held in >= its quantity
    struct PaletteEntry { std::string item; std::string hex; };
    std::vector<PaletteEntry> unlocked_palette(const std::string& username);  // discovered AND pigment_hex set

    // ---- orphan reap (BUILD orphans reap; extended here so civic account-deletion cannot leak the
    // Studio the way house rows once could) ----------------------------------------------------------
    struct Footprint { int batches = 0, discoveries = 0, cooldowns = 0, paintings = 0; };
    Footprint footprint_of(const std::string& username);
    struct ReapResult { int batches = 0, discoveries = 0, cooldowns = 0, paintings = 0; };
    // Own transaction, same shape as HouseStore::reap_orphan: rolls back whole on any failure. Safe to
    // call on a username with no players row at all (returns a zero result, deletes nothing).
    ReapResult reap_orphan(const std::string& username);

private:
    sqlite3* db_;
    // A bound player's very first game action can legitimately be a gather (never entered their house,
    // never done a lab pickup -- the two other paths that create the row today), so gather and bench
    // find-or-create, same shape as HouseStore::find_or_create_player. Every read-only query below uses
    // the other one instead: looking at a player's studio state must never conjure a row into existence.
    int find_or_create_player(const std::string& username);
    std::optional<int> find_player_id(const std::string& username);
};

} // namespace commons
