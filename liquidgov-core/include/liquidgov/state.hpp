#pragma once

#include "events.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace liquidgov {

struct Identity {
    IdentityId id = IdentityId::None;
    std::string handle;
    bool active = true;
    Timestamp enrolled_at = 0;
};

struct Citizen {
    CitizenId id = CitizenId::None;
    std::string handle;
    bool active = true;
    Timestamp enrolled_at = 0; // citizenship start (accrual)
};

struct Motion {
    MotionId id = MotionId::None;
    CitizenId author = CitizenId::None;
    TopicId topic = TopicId::All;
    std::string title;
    std::string body;
    bool closed = false;
};

struct Seat {
    SeatId id = SeatId::None;
    std::string name;
    TopicId topic = TopicId::All;
    CitizenId holder = CitizenId::None;
};

struct Parcel {
    ParcelId id = ParcelId::None;
    CitizenId holder = CitizenId::None;
    std::string description;
    PoolKind kind = PoolKind::CitizenAllocable;
    Weight bond = 0;
    CitizenId consignee = CitizenId::None;
    BusinessId consignee_business = BusinessId::None;
    std::string consignment_portion;
    std::string consignment_terms;
};

struct Business {
    BusinessId id = BusinessId::None;
    std::string handle;
    std::vector<CitizenId> controllers;
};

// Type only. Offices do not reference this yet (no locality on the appoint path).
struct Locality {
    LocalityId id = LocalityId::National;
    std::string name{"national"};
};

struct Judgment {
    JudgmentId id = JudgmentId::None;
    IdentityId subject = IdentityId::None;
    IdentityId harmed = IdentityId::None;
    JudgmentAuthor author = JudgmentAuthor::Genesis;
    IdentityId author_holder = IdentityId::None;
    std::string law;
    std::string point_on_range;
    std::string reasons;
    std::string outcome;
};

struct Office {
    OfficeId id = OfficeId::None;
    std::string name;
    CitizenId holder = CitizenId::None;
    bool contracted = false;
    TopicId contract_topic = TopicId::All;
};

struct ContractRecord {
    OfficeId office = OfficeId::None;
    MotionId motion = MotionId::None;
    bool followed = false;
    std::string explanation;
};

struct AlignmentScore {
    std::uint32_t followed = 0;
    std::uint32_t total = 0;
    double ratio() const { return total == 0 ? 1.0 : double(followed) / double(total); }
};

struct Tally {
    Weight aye = 0;
    Weight nay = 0;
    Weight abstain = 0;
    Weight silent = 0;
};

struct ApplyError {
    std::string message;
};

class Polity {
public:
    explicit Polity(Timestamp genesis = 0, Weight delegate_ceiling = kUnsetDelegateCeiling);

    std::optional<ApplyError> apply(const LedgerEvent& ev);

    const std::vector<LedgerEvent>& ledger() const { return ledger_; }
    Hash tip() const { return last_hash_; }

    const Identity* identity(IdentityId id) const;
    const Citizen* citizen(CitizenId id) const;
    bool has_standing(IdentityId id) const;
    std::size_t active_identities() const;
    const Motion* motion(MotionId id) const;
    const Seat* seat(SeatId id) const;
    const Parcel* parcel(ParcelId id) const;
    const Business* business(BusinessId id) const;
    const Locality* locality(LocalityId id) const;
    const Office* office(OfficeId id) const;
    const Judgment* judgment(JudgmentId id) const;
    IdentityId genesis_authority() const { return genesis_holder_; }

    CitizenId proxy(CitizenId voter, TopicId topic) const;
    CitizenId resolve_delegate(CitizenId voter, TopicId topic) const;
    Weight vote_points(CitizenId id) const;
    Weight vote_points_at(CitizenId id, Timestamp at) const;
    Weight national_points() const;
    Tally tally(MotionId motion) const;
    AlignmentScore alignment(OfficeId office) const;
    std::size_t active_citizens() const;

    // Majority among aye/nay only. nullopt if no decisive votes.
    std::optional<CastBallot::Choice> majority(MotionId motion) const;

private:
    Timestamp last_ts_ = 0;
    EventId last_event_ = EventId::None;
    Hash last_hash_ = zero_hash();
    Weight delegate_ceiling_ = kUnsetDelegateCeiling;

    std::unordered_map<std::uint64_t, Identity> identities_;
    std::unordered_map<std::uint64_t, Citizen> citizens_;
    std::unordered_map<std::uint64_t, Motion> motions_;
    std::unordered_map<std::uint64_t, Seat> seats_;
    std::unordered_map<std::uint64_t, Parcel> parcels_;
    std::unordered_map<std::uint64_t, Business> businesses_;
    std::unordered_map<std::uint64_t, Locality> localities_;
    std::unordered_map<std::uint64_t, Office> offices_;
    std::unordered_map<std::uint64_t, Judgment> judgments_;
    IdentityId genesis_holder_ = IdentityId::None;
    std::vector<ContractRecord> contract_log_;

    std::unordered_map<std::uint64_t, std::unordered_map<std::uint64_t, CitizenId>> delegations_;

    bool reaches(CitizenId from, CitizenId to, TopicId topic) const;
    Weight held_points_at(CitizenId delegate, TopicId topic, Timestamp at) const;
    std::unordered_map<std::uint64_t, std::unordered_map<std::uint64_t, CastBallot::Choice>> ballots_;

    std::vector<LedgerEvent> ledger_;

    bool known_active(CitizenId id) const;
    std::optional<ApplyError> check_meta(const LedgerEvent& ev) const;
};

} // namespace liquidgov
