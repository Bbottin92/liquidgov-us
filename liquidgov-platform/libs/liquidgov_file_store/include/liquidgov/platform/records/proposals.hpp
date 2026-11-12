#pragma once
// records/proposals.hpp: the proposals record family's types and helpers (moved out of file_store.hpp 2026-11-13, code
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

enum class BallotKind : int {
    YesNo = 0,   // existing For / Against / Abstain
    Number = 1,  // pick a value (deadline in days, a budget, a cap)
    Date = 2,    // pick a calendar date
    Choice = 3,  // pick one of the author's labels
    Scale = 4,   // 0–100 slider
};

// Reserved answers (Brief 7 item 4). Whole-value, exact-case matches only.
inline constexpr const char* kBallotSkip = "skip"; // Skip / no preference, any kind but YesNo (YesNo uses Abstain)
inline constexpr const char* kBallotNone = "none"; // None of these, Choice only

// True when no two labels are the same ignoring ASCII letter case (Brief 7 item 5: "Park" and "park" at create).
bool ballot_options_unique(const std::vector<std::string>& options);

struct BallotSpec {
    BallotKind kind = BallotKind::YesNo;
    std::string unit;   // Number: "days", "$" — empty is fine
    double min_v = 0;   // Number: both 0 means unbounded (still clamped)
    double max_v = 0;
    double step = 1;
    std::vector<std::string> options; // Choice: 2–12 closed, 1–48 if open_choice; Scale: optional left/right
    bool open_choice = false;         // people may append a choice (non-author: one each)
    std::vector<std::string> option_authors; // parallel to options; empty = creator-seeded
};

struct Proposal {
    std::uint64_t id = 0;
    std::string author;
    std::int64_t ts = 0;
    std::string title;
    std::string body;
    BallotSpec ballot;
    bool is_office = false;
    std::string jurisdiction; // /officials only. A city's proposal does not use this.
    std::string city;         // city_key, empty when the proposal names no city
    std::string who_acts;     // "who could act on it"; empty when the author left it blank
    std::vector<std::string> tags; // department/council spellings from that city's list
};

// A comment on a proposal. Same fields as a post reply. Public to every
// signed-in member, which is who can see proposals.
struct ProposalReply {
    std::uint64_t id = 0;
    std::uint64_t proposal_id = 0;
    std::string author;
    std::int64_t ts = 0;
    std::string text;
};

enum class ProposalVote : int {
    Yes = 0,
    No = 1,
    Abstain = 2,
};

struct ProposalTally {
    int yes = 0;
    int no = 0;
    int abstain = 0;
};

// For Number / Date / Scale / Choice. YesNo keeps using ProposalTally.
struct BallotTally {
    int n = 0;
    double median = 0;
    double lo = 0;
    double hi = 0;
    std::string median_date;
    std::string lo_date;
    std::string hi_date;
    std::vector<int> option_counts;
    int skipped = 0;      // "skip" answers: not in n, the median, the range or option_counts
    int none_of_these = 0; // Choice "none" answers: counted in n, not in option_counts
};

}  // namespace liquidgov::platform
