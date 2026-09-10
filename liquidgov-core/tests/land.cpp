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
    put(p, id, 1, EnrollCitizen{CitizenId{1}, "alice"});
    put(p, id, 2, EnrollCitizen{CitizenId{2}, "bob"});

    put(p, id, 3, RecordParcel{ParcelId{1}, CitizenId{1}, "lot", PoolKind::CitizenAllocable, 100});
    assert(p.parcel(ParcelId{1})->bond == 100);
    put(p, id, 4,
        ConsignParcel{ParcelId{1}, CitizenId{1}, CitizenId{2}, BusinessId::None, "all", "market"});
    assert(p.parcel(ParcelId{1})->holder == CitizenId{1});
    assert(p.parcel(ParcelId{1})->consignee == CitizenId{2});
    assert(p.parcel(ParcelId{1})->bond == 100);

    put(p, id, 5, RecordParcel{ParcelId{2}, CitizenId::None, "woods", PoolKind::ReservedNature, 0});
    LedgerEvent bad;
    bad.id = id;
    bad.at = 6;
    bad.act = ConsignParcel{ParcelId{2}, CitizenId{1}, CitizenId{2}, BusinessId::None, "all", "x"};
    assert(p.apply(bad).has_value());

    put(p, id, 7, DedicateParcel{ParcelId{1}, CitizenId{1}, PoolKind::ReservedNature});
    assert(p.parcel(ParcelId{1})->kind == PoolKind::ReservedNature);
    assert(p.parcel(ParcelId{1})->holder == CitizenId::None);
    assert(p.parcel(ParcelId{1})->consignee == CitizenId::None);

    put(p, id, 8, RecordParcel{ParcelId{3}, CitizenId::None, "empty", PoolKind::CitizenAllocable, 0});
    put(p, id, 9, ClaimVacantParcel{ParcelId{3}, CitizenId{2}});
    assert(p.parcel(ParcelId{3})->holder == CitizenId{2});

    put(p, id, 10, RecordParcel{ParcelId{4}, CitizenId{1}, "yard", PoolKind::CitizenAllocable, 50});
    put(p, id, 11, DedicateParcel{ParcelId{4}, CitizenId{1}, PoolKind::Commons});
    put(p, id, 12, OpenBusiness{BusinessId{1}, CitizenId{2}, "coop"});
    put(p, id, 13, RecordParcel{ParcelId{5}, CitizenId{1}, "shop", PoolKind::CitizenAllocable, 10});
    put(p, id, 14,
        ConsignParcel{ParcelId{5}, CitizenId{1}, CitizenId::None, BusinessId{1}, "all", "lease"});
    assert(p.parcel(ParcelId{5})->consignee_business == BusinessId{1});
    put(p, id, 15, JoinBusiness{BusinessId{1}, CitizenId{1}});
    assert(p.business(BusinessId{1})->controllers.size() == 2);
    assert(p.parcel(ParcelId{4})->kind == PoolKind::Commons);
    assert(p.parcel(ParcelId{4})->holder == CitizenId{1});
    assert(p.parcel(ParcelId{4})->bond == 50);

    {
        LedgerEvent ev;
        ev.id = EventId{40};
        ev.at = 40;
        ev.act = RecordParcel{ParcelId{9}, CitizenId{1}, "north lot"};
        const auto bytes = encode_for_hash(ev);
        auto back = decode_for_hash(bytes);
        assert(back.has_value());
        assert(encode_for_hash(*back) == bytes);
        LedgerEvent bonded = ev;
        bonded.act = RecordParcel{ParcelId{9}, CitizenId{1}, "north lot",
                                 PoolKind::CitizenAllocable, 100};
        assert(encode_for_hash(bonded) != bytes);
    }

    std::cout << "land ok\n";
    return 0;
}
