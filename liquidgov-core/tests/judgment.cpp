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
    put(p, id, 1, EnrollIdentity{IdentityId{1}, "alex"});
    put(p, id, 2, EnrollIdentity{IdentityId{2}, "blair"});
    put(p, id, 3, AssertGenesisAuthority{IdentityId{1}});
    assert(p.genesis_authority() == IdentityId{1});

    put(p, id, 4,
        RecordJudgment{JudgmentId{1}, IdentityId{2}, IdentityId{1}, JudgmentAuthor::Genesis,
                       IdentityId{1}, "do not steal", "restorative", "return the tool"});
    assert(p.judgment(JudgmentId{1})->subject == IdentityId{2});

    LedgerEvent judge;
    judge.id = id;
    judge.at = 5;
    judge.act = RecordJudgment{JudgmentId{2}, IdentityId{2}, IdentityId::None, JudgmentAuthor::Judge,
                               IdentityId{1}, "do not steal", "x", "y"};
    assert(p.apply(judge).has_value());

    put(p, id, 6, NoteJudgmentOutcome{JudgmentId{1}, IdentityId{1}, "tool returned"});
    assert(p.judgment(JudgmentId{1})->outcome == "tool returned");

    put(p, id, 7, RenounceGenesisAuthority{IdentityId{1}});
    assert(p.genesis_authority() == IdentityId::None);
    LedgerEvent after;
    after.id = id;
    after.at = 8;
    after.act = RecordJudgment{JudgmentId{3}, IdentityId{2}, IdentityId::None, JudgmentAuthor::Genesis,
                               IdentityId{1}, "do not steal", "x", "y"};
    assert(p.apply(after).has_value());

    std::cout << "judgment ok\n";
    return 0;
}
