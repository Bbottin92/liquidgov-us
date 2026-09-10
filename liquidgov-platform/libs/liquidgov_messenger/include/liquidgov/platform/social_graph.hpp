#pragma once

#include "liquidgov/platform/account.hpp"

// The dependency messenger has on the profiles/friends and
// age-determination slices, neither of which is built as code yet.
// Left abstract the same way BreachChecker and MalwareScanner are: a
// real deployment backs this with the actual FriendEdge table and the
// real AgeAssurance predicate, never a client-supplied flag.

namespace liquidgov::platform {

class SocialGraph {
public:
    virtual ~SocialGraph() = default;
    virtual bool are_friends(AccountId a, AccountId b) const = 0;

    // Fail-safe: any real implementation must default an account with
    // no assurance record to true, per the age-determination design.
    // Not enforced by this interface — it's a property the real
    // implementation must uphold, the same way it can't be enforced
    // that a real BreachChecker actually checks anything.
    virtual bool is_minor(AccountId account) const = 0;
};

} // namespace liquidgov::platform
