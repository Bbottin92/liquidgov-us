#pragma once

#include "liquidgov/hash.hpp"
#include "liquidgov/ids.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace liquidgov {

// Verified person: standing, court party, Track 2 eligibility. No vote points.
struct EnrollIdentity {
    IdentityId id;
    std::string handle;
};

// Opt-in on an existing identity. Same numeric id becomes CitizenId.
struct GrantCitizenship {
    IdentityId id;
};

// Combined act, still live (variant index 0). apply() writes identity then
// citizenship so replay has standing. Not decode-only: apply() is history and
// live; rejecting this act would refuse every pre-split ledger.
struct EnrollCitizen {
    CitizenId id;
    std::string handle;
};

struct ResignCitizen {
    CitizenId id;
};

struct SetDelegation {
    CitizenId from;
    CitizenId to; // None clears. Full weight to one proxy per topic.
    TopicId topic;
};

struct FileMotion {
    MotionId id;
    CitizenId author;
    TopicId topic;
    std::string title;
    std::string body;
};

struct CastBallot {
    MotionId motion;
    CitizenId voter;
    enum class Choice : std::uint8_t { Aye, Nay, Abstain } choice;
};

struct OpenSeat {
    SeatId id;
    std::string name;
    TopicId topic;
};

struct ClaimSeat {
    SeatId seat;
    CitizenId claimant;
};

struct VacateSeat {
    SeatId seat;
    CitizenId by;
};

struct RecordParcel {
    ParcelId id;
    CitizenId holder; // None = unclaimed / not owned (nature)
    std::string description;
    PoolKind kind = PoolKind::CitizenAllocable;
    Weight bond = 0;
};

struct ConsignParcel {
    ParcelId parcel;
    CitizenId from;
    CitizenId to = CitizenId::None;
    BusinessId to_business = BusinessId::None;
    std::string portion;
    std::string terms;
};

struct OpenBusiness {
    BusinessId id;
    CitizenId opener;
    std::string handle;
};

struct JoinBusiness {
    BusinessId business;
    CitizenId joiner;
};

struct AssertGenesisAuthority {
    IdentityId holder;
};

struct RenounceGenesisAuthority {
    IdentityId by;
};

struct RecordJudgment {
    JudgmentId id;
    IdentityId subject;
    IdentityId harmed = IdentityId::None;
    JudgmentAuthor author = JudgmentAuthor::Genesis;
    IdentityId author_holder = IdentityId::None;
    std::string law;
    std::string point_on_range;
    std::string reasons;
};

struct NoteJudgmentOutcome {
    JudgmentId judgment;
    IdentityId by = IdentityId::None;
    std::string outcome;
};

// Interim: holder or consignee may end it. §8 does not name who terminates.
struct ReleaseConsignment {
    ParcelId parcel;
    CitizenId by;
};

struct DedicateParcel {
    ParcelId parcel;
    CitizenId by;
    PoolKind destination;
};

struct ClaimVacantParcel {
    ParcelId parcel;
    CitizenId claimant;
};

struct TransferParcel {
    ParcelId id;
    CitizenId from;
    CitizenId to;
};

struct OpenOffice {
    OfficeId id;
    std::string name;
};

struct AppointOffice {
    OfficeId office;
    CitizenId appointee;
};

struct RemoveOffice {
    OfficeId office;
    CitizenId by;
};

// Officials' Contract: holder pledges to carry the liquid majority on a topic.
struct BindContract {
    OfficeId office;
    CitizenId official;
    TopicId topic;
};

// After a motion on that topic closes, record whether the official followed.
// Deviation requires a non-empty explanation or the event is rejected.
struct NoteAlignment {
    OfficeId office;
    MotionId motion;
    bool followed;
    std::string explanation;
};

using CivicAct = std::variant<
    EnrollCitizen,
    ResignCitizen,
    SetDelegation,
    FileMotion,
    CastBallot,
    OpenSeat,
    ClaimSeat,
    VacateSeat,
    RecordParcel,
    TransferParcel,
    OpenOffice,
    AppointOffice,
    RemoveOffice,
    BindContract,
    NoteAlignment,
    EnrollIdentity,
    GrantCitizenship,
    ConsignParcel,
    ReleaseConsignment,
    DedicateParcel,
    ClaimVacantParcel,
    OpenBusiness,
    JoinBusiness,
    AssertGenesisAuthority,
    RecordJudgment,
    NoteJudgmentOutcome,
    RenounceGenesisAuthority>;

struct LedgerEvent {
    EventId id = EventId::None;
    Timestamp at = 0;
    Hash prev_hash = zero_hash();
    Hash content_hash = zero_hash();
    CivicAct act{};
};

// Canonical bytes for hashing (id, at, prev_hash, act). content_hash is the digest.
// On disk a record is [4-byte length][these bytes][32-byte content_hash], so
// the length prefix is always >= 32. A prefix smaller than that is corrupt,
// not a legal encoding of a smaller event.
std::string encode_for_hash(const LedgerEvent& ev);
std::optional<LedgerEvent> decode_for_hash(std::string_view bytes);
Hash hash_event(const LedgerEvent& ev);

} // namespace liquidgov
