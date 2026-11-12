#pragma once
// records/communities.hpp: the communities record family's types and helpers (moved out of file_store.hpp 2026-11-13, code
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

// A fixed, curated top level for browsing communities -- fixed so the
// directory stays navigable as the platform grows instead of accumulating
// near-duplicate categories ("Gaming" vs "gaming" vs "Games") the way a
// fully open list would. Subcategory (below) is deliberately not an enum:
// that's the axis meant to stay open for people to actually name.
enum class CommunityCategory : int {
    LocalCivic = 0,
    PolicyGovernment = 1,
    ArtsCulture = 2,
    HobbiesInterests = 3,
    SportsRecreation = 4,
    Technology = 5,
    BusinessTrade = 6,
    LearningEducation = 7,
    SupportWellbeing = 8,
    FaithSpirituality = 9,
    General = 10,
};

struct Community {
    std::string slug;
    std::string name;
    std::string owner;
    std::string about;
    std::int64_t created_at = 0;
    CommunityCategory category = CommunityCategory::General;
    // Free text, not an enum -- e.g. "Neighborhood Watch" under Local &
    // Civic. Deliberately open for whoever creates a community to name
    // (see CommunityCategory's own comment); subcategories_of() surfaces
    // whatever's already in use under a category so a new community can
    // reuse an existing name instead of always minting a new one.
    std::string subcategory;
    // Free text the group's starter typed, e.g. "Fresno, CA". Empty = no
    // city. A city on LiquidGov is a label people chose so neighbours can
    // find each other, not a government or an AVND locality. Groups with
    // the same city_key() are listed together on /city/<key>.
    std::string city;
};

}  // namespace liquidgov::platform
