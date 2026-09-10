#pragma once

#include <string>
#include <string_view>

namespace liquidgov {

// Natural-law filter applied to motions *before* they enter the ledger.
// Governance may not authorize what the three laws forbid.
enum class LawKind { Steal, Damage, DepriveLiberty };

struct LawVerdict {
    bool allowed = true;
    LawKind violated = LawKind::Steal;
    std::string reason;
};

class ThreeLaws {
public:
    // A motion text that proposes theft, damage, or deprivation of liberty is rejected.
    // This is a conservative keyword/intent gate for the core; a later layer can
    // replace it with a structured claim language. The invariant is the same:
    // the ledger never records a motion that authorizes a forbidden act.
    static LawVerdict examine(std::string_view title, std::string_view body);
};

} // namespace liquidgov
