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
    // Accrual: no complete year → 0 points.
    {
        Polity p;
        EventId id{1};
        put(p, id, 1, EnrollCitizen{CitizenId{1}, "alice"});
        assert(p.vote_points(CitizenId{1}) == 0);
        put(p, id, 1 + kSecondsPerYear, EnrollCitizen{CitizenId{2}, "bob"});
        assert(p.vote_points(CitizenId{1}) == 1);
        assert(p.vote_points(CitizenId{2}) == 0);
    }

    // Reject-at-set if to already reaches from. Dump remains if a cycle
    // were somehow present (assumption: spec is graph constraint).
    {
        Polity p;
        EventId id{1};
        put(p, id, 1, EnrollCitizen{CitizenId{1}, "alice"});
        put(p, id, 2, EnrollCitizen{CitizenId{2}, "bob"});
        put(p, id, 2 * kSecondsPerYear, SetDelegation{CitizenId{1}, CitizenId{2}, TopicId::All});
        LedgerEvent back;
        back.id = id;
        back.at = 2 * kSecondsPerYear + 1;
        back.act = SetDelegation{CitizenId{2}, CitizenId{1}, TopicId::All};
        auto err = p.apply(back);
        assert(err.has_value());
        assert(err->message.find("cycle") != std::string::npos);
        assert(p.proxy(CitizenId{2}, TopicId::All) == CitizenId::None);
    }

    // Ceiling: reject an edge that would put a delegate over the cap.
    {
        Polity p(0, /*ceiling*/ 2);
        EventId id{1};
        put(p, id, 1, EnrollCitizen{CitizenId{1}, "alice"});
        put(p, id, 2, EnrollCitizen{CitizenId{2}, "bob"});
        LedgerEvent e;
        e.id = id;
        e.at = 2 * kSecondsPerYear + 1;
        e.act = SetDelegation{CitizenId{1}, CitizenId{2}, TopicId::All};
        auto err = p.apply(e);
        assert(err.has_value());
        assert(err->message.find("ceiling") != std::string::npos);
        assert(p.proxy(CitizenId{1}, TopicId::All) == CitizenId::None);
    }

    std::cout << "delegation ok\n";
    return 0;
}
