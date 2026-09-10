#include "liquidgov/state.hpp"
#include "liquidgov/store.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace liquidgov;

int main() {
    EventId next_id{1};
    Timestamp t = 1;
    Polity p;

    auto apply = [&](CivicAct act) {
        LedgerEvent e;
        e.id = next_id;
        e.at = t++;
        e.act = std::move(act);
        next_id = EventId{static_cast<std::uint64_t>(next_id) + 1};
        auto err = p.apply(e);
        if (err) {
            std::cerr << err->message << "\n";
            assert(false && "unexpected apply error");
        }
    };

    apply(EnrollCitizen{CitizenId{1}, "alice"});
    apply(EnrollCitizen{CitizenId{2}, "bob"});
    apply(EnrollCitizen{CitizenId{3}, "cara"});
    t = 2 * kSecondsPerYear + 10;

    apply(SetDelegation{CitizenId{1}, CitizenId{2}, TopicId::All});
    apply(FileMotion{MotionId{1}, CitizenId{2}, TopicId::All, "Open the commons",
                     "Voluntary use of the wall."});
    apply(CastBallot{MotionId{1}, CitizenId{2}, CastBallot::Choice::Aye});

    Tally tl = p.tally(MotionId{1});
    assert(tl.aye == 4); // alice's 2 points follow bob's 2
    assert(tl.nay == 0);
    assert(tl.silent == 2); // cara

    apply(SetDelegation{CitizenId{1}, CitizenId::None, TopicId::All});
    apply(CastBallot{MotionId{1}, CitizenId{1}, CastBallot::Choice::Nay});
    tl = p.tally(MotionId{1});
    assert(tl.aye == 2);
    assert(tl.nay == 2);

    // Rejected acts do not consume a ledger sequence number.
    LedgerEvent seizure;
    seizure.id = next_id;
    seizure.at = t;
    seizure.act = FileMotion{MotionId{2}, CitizenId{2}, TopicId::All,
                             "Seize property downtown", "confiscate vacant lots"};
    auto bad = p.apply(seizure);
    assert(bad.has_value());

    apply(RecordParcel{ParcelId{1}, CitizenId{1}, "north lot"});
    apply(TransferParcel{ParcelId{1}, CitizenId{1}, CitizenId{2}});
    assert(p.parcel(ParcelId{1})->holder == CitizenId{2});

    apply(OpenOffice{OfficeId{1}, "clerk"});
    apply(AppointOffice{OfficeId{1}, CitizenId{2}});
    apply(BindContract{OfficeId{1}, CitizenId{2}, TopicId::All});
    apply(NoteAlignment{OfficeId{1}, MotionId{1}, true, ""});
    assert(p.alignment(OfficeId{1}).followed == 1);

    Hash tip = p.tip();
    assert(tip != zero_hash());
    assert(p.ledger().back().content_hash == tip);
    assert(p.ledger().front().prev_hash == zero_hash());

    auto tmp = std::filesystem::temp_directory_path() / "liquidgov-smoke.ledger";
    std::filesystem::remove(tmp);
    {
        auto store = GovernanceStore::load_or_create(tmp);
        EventId id{1};
        Timestamp ts = 1;
        auto put = [&](CivicAct act) {
            LedgerEvent e;
            e.id = id;
            e.at = ts++;
            e.act = std::move(act);
            id = EventId{static_cast<std::uint64_t>(id) + 1};
            auto err = store.submit(e);
            assert(!err);
        };
        put(EnrollCitizen{CitizenId{1}, "alice"});
        put(EnrollCitizen{CitizenId{2}, "bob"});
        ts = 2 * kSecondsPerYear + 10;
        put(SetDelegation{CitizenId{1}, CitizenId{2}, TopicId::All});
        put(FileMotion{MotionId{1}, CitizenId{2}, TopicId::All, "Open the commons", "ok"});
        put(CastBallot{MotionId{1}, CitizenId{2}, CastBallot::Choice::Aye});
        assert(store.polity().tally(MotionId{1}).aye == 4);
    }
    {
        auto store = GovernanceStore::load_or_create(tmp);
        assert(store.polity().active_citizens() == 2);
        assert(store.polity().tally(MotionId{1}).aye == 4);
        assert(store.polity().ledger().size() == 5);
    }
    std::filesystem::remove(tmp);

    std::cout << "smoke ok; ledger events=" << p.ledger().size()
              << " citizens=" << p.active_citizens()
              << " tip=" << hex(tip).substr(0, 16) << "\n";
    return 0;
}
