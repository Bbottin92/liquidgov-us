#pragma once
// records/pains.hpp: the pains record family's types and helpers (moved out of file_store.hpp 2026-11-13, code
// moved, not rewritten). The LGP1 format of every record type stays in file_store.hpp's header comment.

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace liquidgov::platform {

// ---- Wishes and pain points (Brandon 2026-11-06; first slice per Grok's review) ----
// The site notices "I wish ...", "I hate when ...", "I'm tired of ..." in a
// person's OWN new public words (Anybody posts, proposals, proposal comments),
// and offers each as a suggestion they Keep or Forget. A kept one starts as a
// private box on their own map. Flow shows that one subject to people who may
// see them; Still makes it private again. The list itself stays the owner's.
// Nothing is sent to an outside AI. Only a short phrase and the source id are
// stored, never a copy of the text.
struct PainHit {
    std::string kind;    // "wish" or "pain"
    std::string phrase;  // the clause after the trigger, <= 12 words / 80 bytes
};

struct PainScan {
    std::vector<PainHit> hits;
    bool crisis = false; // words like "I wish I wasn't here": store nothing, show 988
};

// Pure function, no I/O. At most 3 hits. Skips text inside quotes and lines
// starting with '>'. "someone should" / "why can't" are deliberately not
// triggers (they match sarcasm and debate).
PainScan find_pain_points(const std::string& text);

// Lowercase words of 4+ letters that are not filler, for matching a phrase to
// departments, proposals and groups ("buses" also matches "bus").
std::vector<std::string> pain_keywords(const std::string& phrase);

// The subject two Flowed boxes share: case-folded, with whitespace runs
// collapsed. Punctuation stays, so a shared word is not the same subject.
std::string pain_subject_key(const std::string& phrase);

// True when any keyword appears as a word in `text` (case-folded; a simple
// plural rule so "buses" matches "bus" and "trees" matches "tree").
bool pain_words_overlap(const std::vector<std::string>& keywords, const std::string& text);

struct PainPoint {
    std::uint64_t id = 0;
    std::string owner;
    std::string kind;    // wish | pain
    std::string phrase;
    std::string source;  // post:<id> | prop:<id> | prply:<id>
    std::int64_t ts = 0;
    bool kept = false;   // false = a suggestion; unkept ones expire after 30 days
    bool flowed = false; // true only after the owner Flows a kept box
    std::vector<std::string> extra_fields; // a newer version's fields after flowed, kept verbatim (see MapPrefs)
};

}  // namespace liquidgov::platform
