#include "liquidgov/state.hpp"

#include <cassert>
#include <iostream>

using namespace liquidgov;

static void put(Polity& p, EventId& id, Timestamp at, CivicAct act) {
    LedgerEvent e;
    e.id = id;
    e.at = at;
    e.act = std::move(act);
    id = EventId{static_cast<std::uint64_t>(id) + 1};
    auto err = p.apply(e);
    if (err) {
        std::cerr << err->message << "\n";
        assert(false);
    }
}

int main() {
    Polity p;
    EventId id{1};

    put(p, id, 1, EnrollIdentity{IdentityId{10}, "resident"});
    assert(p.has_standing(IdentityId{10}));
    assert(p.active_identities() == 1);
    assert(p.active_citizens() == 0);
    assert(p.vote_points(as_citizen(IdentityId{10})) == 0);
    assert(p.citizen(as_citizen(IdentityId{10})) == nullptr);

    LedgerEvent vote;
    vote.id = id;
    vote.at = 2 * kSecondsPerYear;
    vote.act = SetDelegation{as_citizen(IdentityId{10}), CitizenId::None, TopicId::All};
    assert(p.apply(vote).has_value()); // not a citizen

    put(p, id, 2 * kSecondsPerYear, GrantCitizenship{IdentityId{10}});
    assert(p.active_citizens() == 1);
    assert(p.vote_points(as_citizen(IdentityId{10})) == 0); // just granted

    put(p, id, 3 * kSecondsPerYear, EnrollCitizen{CitizenId{1}, "alice"});
    assert(p.has_standing(as_identity(CitizenId{1})));
    assert(p.active_citizens() == 2);
    assert(p.vote_points(CitizenId{1}) == 0);

    LedgerEvent again;
    again.id = id;
    again.at = 3 * kSecondsPerYear + 1;
    again.act = GrantCitizenship{IdentityId{10}};
    assert(p.apply(again).has_value());

    std::cout << "identity ok\n";
    return 0;
}
