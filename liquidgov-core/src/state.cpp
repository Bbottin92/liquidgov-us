#include "liquidgov/state.hpp"
#include "liquidgov/law.hpp"

#include <algorithm>
#include <type_traits>
#include <unordered_set>

namespace liquidgov {
namespace {

std::uint64_t raw(IdentityId id) { return static_cast<std::uint64_t>(id); }
std::uint64_t raw(CitizenId id) { return static_cast<std::uint64_t>(id); }
std::uint64_t raw(MotionId id) { return static_cast<std::uint64_t>(id); }
std::uint64_t raw(SeatId id) { return static_cast<std::uint64_t>(id); }
std::uint64_t raw(ParcelId id) { return static_cast<std::uint64_t>(id); }
std::uint64_t raw(BusinessId id) { return static_cast<std::uint64_t>(id); }
std::uint64_t raw(OfficeId id) { return static_cast<std::uint64_t>(id); }
std::uint64_t raw(TopicId id) { return static_cast<std::uint64_t>(id); }
std::uint64_t raw(EventId id) { return static_cast<std::uint64_t>(id); }
std::uint64_t raw(JudgmentId id) { return static_cast<std::uint64_t>(id); }
std::uint64_t raw(LocalityId id) { return static_cast<std::uint64_t>(id); }

} // namespace

Polity::Polity(Timestamp genesis, Weight delegate_ceiling)
    : last_ts_(genesis), delegate_ceiling_(delegate_ceiling) {
    localities_[raw(LocalityId::National)] = Locality{LocalityId::National, "national"};
}

const Identity* Polity::identity(IdentityId id) const {
    auto it = identities_.find(raw(id));
    return it == identities_.end() ? nullptr : &it->second;
}

const Citizen* Polity::citizen(CitizenId id) const {
    auto it = citizens_.find(raw(id));
    return it == citizens_.end() ? nullptr : &it->second;
}

const Motion* Polity::motion(MotionId id) const {
    auto it = motions_.find(raw(id));
    return it == motions_.end() ? nullptr : &it->second;
}

const Seat* Polity::seat(SeatId id) const {
    auto it = seats_.find(raw(id));
    return it == seats_.end() ? nullptr : &it->second;
}

const Parcel* Polity::parcel(ParcelId id) const {
    auto it = parcels_.find(raw(id));
    return it == parcels_.end() ? nullptr : &it->second;
}

const Business* Polity::business(BusinessId id) const {
    auto it = businesses_.find(raw(id));
    return it == businesses_.end() ? nullptr : &it->second;
}

const Locality* Polity::locality(LocalityId id) const {
    auto it = localities_.find(raw(id));
    return it == localities_.end() ? nullptr : &it->second;
}

const Judgment* Polity::judgment(JudgmentId id) const {
    auto it = judgments_.find(raw(id));
    return it == judgments_.end() ? nullptr : &it->second;
}

const Office* Polity::office(OfficeId id) const {
    auto it = offices_.find(raw(id));
    return it == offices_.end() ? nullptr : &it->second;
}

bool Polity::has_standing(IdentityId id) const {
    const Identity* i = identity(id);
    return i && i->active;
}

bool Polity::known_active(CitizenId id) const {
    const Citizen* c = citizen(id);
    return c && c->active;
}

std::size_t Polity::active_identities() const {
    std::size_t n = 0;
    for (const auto& [_, i] : identities_) {
        if (i.active) ++n;
    }
    return n;
}

std::size_t Polity::active_citizens() const {
    std::size_t n = 0;
    for (const auto& [_, c] : citizens_) {
        if (c.active) ++n;
    }
    return n;
}

CitizenId Polity::proxy(CitizenId voter, TopicId topic) const {
    auto fit = delegations_.find(raw(voter));
    if (fit == delegations_.end()) return CitizenId::None;
    auto tit = fit->second.find(raw(topic));
    if (tit != fit->second.end()) return tit->second;
    if (topic != TopicId::All) {
        auto git = fit->second.find(raw(TopicId::All));
        if (git != fit->second.end()) return git->second;
    }
    return CitizenId::None;
}

Weight Polity::vote_points_at(CitizenId id, Timestamp at) const {
    const Citizen* c = citizen(id);
    if (!c || !c->active) return 0;
    if (at < c->enrolled_at) return 0;
    return (at - c->enrolled_at) / kSecondsPerYear;
}

Weight Polity::vote_points(CitizenId id) const { return vote_points_at(id, last_ts_); }

Weight Polity::national_points() const {
    Weight n = 0;
    for (const auto& [id, c] : citizens_) {
        if (c.active) n += vote_points(CitizenId{id});
    }
    return n;
}

bool Polity::reaches(CitizenId origin, CitizenId start, TopicId topic) const {
    std::unordered_set<std::uint64_t> seen;
    CitizenId cur = start;
    while (cur != CitizenId::None && known_active(cur)) {
        if (cur == origin) return true;
        if (!seen.insert(raw(cur)).second) return false;
        cur = proxy(cur, topic);
    }
    return false;
}

Weight Polity::held_points_at(CitizenId delegate, TopicId topic, Timestamp at) const {
    Weight w = 0;
    for (const auto& [id, c] : citizens_) {
        if (!c.active) continue;
        CitizenId src{id};
        if (resolve_delegate(src, topic) == delegate) w += vote_points_at(src, at);
    }
    return w;
}

CitizenId Polity::resolve_delegate(CitizenId voter, TopicId topic) const {
    std::unordered_set<std::uint64_t> seen;
    CitizenId cur = voter;
    while (true) {
        if (!seen.insert(raw(cur)).second) {
            // Cycle: the source votes for themselves. Liquidity never traps weight.
            return voter;
        }
        CitizenId next = proxy(cur, topic);
        if (next == CitizenId::None || !known_active(next)) return cur;
        cur = next;
    }
}

Tally Polity::tally(MotionId mid) const {
    Tally t{};
    const Motion* m = motion(mid);
    if (!m) return t;

    auto bit = ballots_.find(raw(mid));
    const auto* ballots = bit == ballots_.end() ? nullptr : &bit->second;

    auto choice_of = [&](CitizenId id) -> std::optional<CastBallot::Choice> {
        if (!ballots) return std::nullopt;
        auto it = ballots->find(raw(id));
        if (it == ballots->end()) return std::nullopt;
        return it->second;
    };
    auto add = [&](Weight w, std::optional<CastBallot::Choice> ch) {
        if (w == 0) return;
        if (!ch) {
            t.silent += w;
            return;
        }
        switch (*ch) {
        case CastBallot::Choice::Aye: t.aye += w; break;
        case CastBallot::Choice::Nay: t.nay += w; break;
        case CastBallot::Choice::Abstain: t.abstain += w; break;
        }
    };

    for (const auto& [id, c] : citizens_) {
        if (!c.active) continue;
        CitizenId src{id};
        const Weight have = vote_points(src);
        const CitizenId terminal = resolve_delegate(src, m->topic);
        auto ch = choice_of(terminal);
        if (!ch) ch = choice_of(src);
        add(have, ch);
    }
    return t;
}

std::optional<CastBallot::Choice> Polity::majority(MotionId motion) const {
    Tally t = tally(motion);
    if (t.aye == t.nay) return std::nullopt;
    return t.aye > t.nay ? CastBallot::Choice::Aye : CastBallot::Choice::Nay;
}

AlignmentScore Polity::alignment(OfficeId office) const {
    AlignmentScore s{};
    for (const auto& r : contract_log_) {
        if (r.office != office) continue;
        ++s.total;
        if (r.followed) ++s.followed;
    }
    return s;
}

std::optional<ApplyError> Polity::check_meta(const LedgerEvent& ev) const {
    if (raw(ev.id) != raw(last_event_) + 1) {
        return ApplyError{"event id must be last+1"};
    }
    if (ev.at < last_ts_) {
        return ApplyError{"timestamp must be monotonic"};
    }
    return std::nullopt;
}

std::optional<ApplyError> Polity::apply(const LedgerEvent& ev) {
    if (auto e = check_meta(ev)) return e;

    LedgerEvent preview = ev;
    preview.prev_hash = last_hash_;
    preview.content_hash = hash_event(preview);
    if (ev.content_hash != zero_hash() && ev.content_hash != preview.content_hash) {
        return ApplyError{"content hash mismatch"};
    }
    if (ev.prev_hash != zero_hash() && ev.prev_hash != last_hash_) {
        return ApplyError{"prev hash mismatch"};
    }

    std::optional<ApplyError> err;

    std::visit(
        [&](const auto& act) {
            using T = std::decay_t<decltype(act)>;

            if constexpr (std::is_same_v<T, EnrollCitizen>) {
                if (act.id == CitizenId::None || act.handle.empty()) {
                    err = ApplyError{"invalid citizen"};
                    return;
                }
                if (identities_.count(raw(act.id)) || citizens_.count(raw(act.id))) {
                    err = ApplyError{"person exists"};
                    return;
                }
                for (const auto& [_, i] : identities_) {
                    if (i.handle == act.handle) {
                        err = ApplyError{"handle taken"};
                        return;
                    }
                }
                identities_[raw(act.id)] =
                    Identity{as_identity(act.id), act.handle, true, ev.at};
                citizens_[raw(act.id)] = Citizen{act.id, act.handle, true, ev.at};
            } else if constexpr (std::is_same_v<T, ResignCitizen>) {
                if (!known_active(act.id)) {
                    err = ApplyError{"not an active citizen"};
                    return;
                }
                citizens_[raw(act.id)].active = false;
                delegations_.erase(raw(act.id));
                for (auto& [_, topics] : delegations_) {
                    for (auto it = topics.begin(); it != topics.end();) {
                        if (it->second == act.id) it = topics.erase(it);
                        else ++it;
                    }
                }
            } else if constexpr (std::is_same_v<T, SetDelegation>) {
                if (!known_active(act.from)) {
                    err = ApplyError{"from is not active"};
                    return;
                }
                if (act.to != CitizenId::None) {
                    if (act.to == act.from) {
                        err = ApplyError{"cannot delegate to self"};
                        return;
                    }
                    if (!known_active(act.to)) {
                        err = ApplyError{"to is not active"};
                        return;
                    }
                    if (reaches(act.from, act.to, act.topic)) {
                        err = ApplyError{"delegation would cycle"};
                        return;
                    }
                    auto prev_to = proxy(act.from, act.topic);
                    delegations_[raw(act.from)][raw(act.topic)] = act.to;
                    if (delegate_ceiling_ != kUnsetDelegateCeiling &&
                        held_points_at(act.to, act.topic, ev.at) > delegate_ceiling_) {
                        if (prev_to == CitizenId::None) {
                            delegations_[raw(act.from)].erase(raw(act.topic));
                        } else {
                            delegations_[raw(act.from)][raw(act.topic)] = prev_to;
                        }
                        err = ApplyError{"delegate exceeds ceiling"};
                        return;
                    }
                } else {
                    auto fit = delegations_.find(raw(act.from));
                    if (fit != delegations_.end()) {
                        fit->second.erase(raw(act.topic));
                        if (fit->second.empty()) delegations_.erase(fit);
                    }
                }
            } else if constexpr (std::is_same_v<T, FileMotion>) {
                if (!known_active(act.author)) {
                    err = ApplyError{"author is not active"};
                    return;
                }
                if (act.id == MotionId::None || act.title.empty()) {
                    err = ApplyError{"invalid motion"};
                    return;
                }
                if (motions_.count(raw(act.id))) {
                    err = ApplyError{"motion exists"};
                    return;
                }
                auto law = ThreeLaws::examine(act.title, act.body);
                if (!law.allowed) {
                    err = ApplyError{law.reason};
                    return;
                }
                motions_[raw(act.id)] = Motion{act.id, act.author, act.topic, act.title, act.body, false};
            } else if constexpr (std::is_same_v<T, CastBallot>) {
                if (!known_active(act.voter)) {
                    err = ApplyError{"voter is not active"};
                    return;
                }
                const Motion* m = motion(act.motion);
                if (!m || m->closed) {
                    err = ApplyError{"motion not open"};
                    return;
                }
                ballots_[raw(act.motion)][raw(act.voter)] = act.choice;
            } else if constexpr (std::is_same_v<T, OpenSeat>) {
                if (act.id == SeatId::None || act.name.empty()) {
                    err = ApplyError{"invalid seat"};
                    return;
                }
                if (seats_.count(raw(act.id))) {
                    err = ApplyError{"seat exists"};
                    return;
                }
                seats_[raw(act.id)] = Seat{act.id, act.name, act.topic, CitizenId::None};
            } else if constexpr (std::is_same_v<T, ClaimSeat>) {
                if (!known_active(act.claimant)) {
                    err = ApplyError{"claimant is not active"};
                    return;
                }
                auto it = seats_.find(raw(act.seat));
                if (it == seats_.end()) {
                    err = ApplyError{"no such seat"};
                    return;
                }
                if (it->second.holder != CitizenId::None) {
                    err = ApplyError{"seat occupied"};
                    return;
                }
                it->second.holder = act.claimant;
            } else if constexpr (std::is_same_v<T, VacateSeat>) {
                auto it = seats_.find(raw(act.seat));
                if (it == seats_.end()) {
                    err = ApplyError{"no such seat"};
                    return;
                }
                if (it->second.holder != act.by) {
                    err = ApplyError{"only holder may vacate"};
                    return;
                }
                it->second.holder = CitizenId::None;
            } else if constexpr (std::is_same_v<T, RecordParcel>) {
                if (act.id == ParcelId::None) {
                    err = ApplyError{"invalid parcel"};
                    return;
                }
                if (parcels_.count(raw(act.id))) {
                    err = ApplyError{"parcel exists"};
                    return;
                }
                if (act.kind == PoolKind::ReservedNature) {
                    if (act.holder != CitizenId::None) {
                        err = ApplyError{"nature land is not owned"};
                        return;
                    }
                } else if (act.holder != CitizenId::None && !known_active(act.holder)) {
                    err = ApplyError{"holder is not active"};
                    return;
                }
                parcels_[raw(act.id)] =
                    Parcel{act.id, act.holder, act.description, act.kind, act.bond, CitizenId::None,
                           BusinessId::None, "", ""};
            } else if constexpr (std::is_same_v<T, TransferParcel>) {
                auto it = parcels_.find(raw(act.id));
                if (it == parcels_.end()) {
                    err = ApplyError{"no such parcel"};
                    return;
                }
                if (it->second.kind == PoolKind::ReservedNature) {
                    err = ApplyError{"nature land is not transactable"};
                    return;
                }
                if (it->second.holder != act.from) {
                    err = ApplyError{"not the holder"};
                    return;
                }
                if (!known_active(act.to)) {
                    err = ApplyError{"recipient not active"};
                    return;
                }
                it->second.holder = act.to;
            } else if constexpr (std::is_same_v<T, OpenOffice>) {
                if (act.id == OfficeId::None || act.name.empty()) {
                    err = ApplyError{"invalid office"};
                    return;
                }
                if (offices_.count(raw(act.id))) {
                    err = ApplyError{"office exists"};
                    return;
                }
                offices_[raw(act.id)] = Office{act.id, act.name, CitizenId::None};
            } else if constexpr (std::is_same_v<T, AppointOffice>) {
                if (!known_active(act.appointee)) {
                    err = ApplyError{"appointee not active"};
                    return;
                }
                auto it = offices_.find(raw(act.office));
                if (it == offices_.end()) {
                    err = ApplyError{"no such office"};
                    return;
                }
                it->second.holder = act.appointee;
            } else if constexpr (std::is_same_v<T, RemoveOffice>) {
                auto it = offices_.find(raw(act.office));
                if (it == offices_.end()) {
                    err = ApplyError{"no such office"};
                    return;
                }
                if (it->second.holder != act.by && act.by != CitizenId::None) {
                    // Removal by the holder only at this layer; a later motion can
                    // appoint a successor after a successful tally.
                    if (it->second.holder != CitizenId::None) {
                        err = ApplyError{"only holder may step down here"};
                        return;
                    }
                }
                it->second.holder = CitizenId::None;
                it->second.contracted = false;
            } else if constexpr (std::is_same_v<T, BindContract>) {
                if (!known_active(act.official)) {
                    err = ApplyError{"official not active"};
                    return;
                }
                auto it = offices_.find(raw(act.office));
                if (it == offices_.end()) {
                    err = ApplyError{"no such office"};
                    return;
                }
                if (it->second.holder != act.official) {
                    err = ApplyError{"only the holder may bind the contract"};
                    return;
                }
                it->second.contracted = true;
                it->second.contract_topic = act.topic;
            } else if constexpr (std::is_same_v<T, NoteAlignment>) {
                auto it = offices_.find(raw(act.office));
                if (it == offices_.end() || !it->second.contracted) {
                    err = ApplyError{"office has no contract"};
                    return;
                }
                const Motion* m = motion(act.motion);
                if (!m) {
                    err = ApplyError{"no such motion"};
                    return;
                }
                if (!act.followed && act.explanation.empty()) {
                    err = ApplyError{"deviation requires explanation"};
                    return;
                }
                contract_log_.push_back(
                    ContractRecord{act.office, act.motion, act.followed, act.explanation});
            } else if constexpr (std::is_same_v<T, EnrollIdentity>) {
                if (act.id == IdentityId::None || act.handle.empty()) {
                    err = ApplyError{"invalid identity"};
                    return;
                }
                if (identities_.count(raw(act.id))) {
                    err = ApplyError{"identity exists"};
                    return;
                }
                for (const auto& [_, i] : identities_) {
                    if (i.handle == act.handle) {
                        err = ApplyError{"handle taken"};
                        return;
                    }
                }
                identities_[raw(act.id)] = Identity{act.id, act.handle, true, ev.at};
            } else if constexpr (std::is_same_v<T, GrantCitizenship>) {
                const Identity* i = identity(act.id);
                if (!i || !i->active) {
                    err = ApplyError{"no standing identity"};
                    return;
                }
                const auto cid = as_citizen(act.id);
                if (citizens_.count(raw(cid))) {
                    err = ApplyError{"already a citizen"};
                    return;
                }
                citizens_[raw(cid)] = Citizen{cid, i->handle, true, ev.at};
            } else if constexpr (std::is_same_v<T, ConsignParcel>) {
                auto it = parcels_.find(raw(act.parcel));
                if (it == parcels_.end()) {
                    err = ApplyError{"no such parcel"};
                    return;
                }
                if (it->second.kind == PoolKind::ReservedNature) {
                    err = ApplyError{"nature land cannot be consigned"};
                    return;
                }
                if (it->second.holder != act.from || !known_active(act.from)) {
                    err = ApplyError{"only title holder may consign"};
                    return;
                }
                const bool to_person = act.to != CitizenId::None;
                const bool to_firm = act.to_business != BusinessId::None;
                if (to_person == to_firm) {
                    err = ApplyError{"consign to exactly one party"};
                    return;
                }
                if (to_person && !known_active(act.to)) {
                    err = ApplyError{"consignee not active"};
                    return;
                }
                if (to_firm && !business(act.to_business)) {
                    err = ApplyError{"no such business"};
                    return;
                }
                it->second.consignee = act.to;
                it->second.consignee_business = act.to_business;
                it->second.consignment_portion = act.portion;
                it->second.consignment_terms = act.terms;
            } else if constexpr (std::is_same_v<T, ReleaseConsignment>) {
                auto it = parcels_.find(raw(act.parcel));
                if (it == parcels_.end()) {
                    err = ApplyError{"no such parcel"};
                    return;
                }
                if (it->second.consignee == CitizenId::None &&
                    it->second.consignee_business == BusinessId::None) {
                    err = ApplyError{"no consignment"};
                    return;
                }
                bool party = act.by == it->second.holder || act.by == it->second.consignee;
                if (it->second.consignee_business != BusinessId::None) {
                    const Business* b = business(it->second.consignee_business);
                    if (b) {
                        for (auto c : b->controllers) {
                            if (c == act.by) party = true;
                        }
                    }
                }
                if (!party) {
                    err = ApplyError{"not a party to the consignment"};
                    return;
                }
                it->second.consignee = CitizenId::None;
                it->second.consignee_business = BusinessId::None;
                it->second.consignment_portion.clear();
                it->second.consignment_terms.clear();
            } else if constexpr (std::is_same_v<T, DedicateParcel>) {
                auto it = parcels_.find(raw(act.parcel));
                if (it == parcels_.end()) {
                    err = ApplyError{"no such parcel"};
                    return;
                }
                if (it->second.holder != act.by || !known_active(act.by)) {
                    err = ApplyError{"only title holder may dedicate"};
                    return;
                }
                if (it->second.kind != PoolKind::CitizenAllocable) {
                    err = ApplyError{"only citizen-allocable land may be dedicated"};
                    return;
                }
                if (act.destination == PoolKind::CitizenAllocable) {
                    err = ApplyError{"dedication needs a destination pool"};
                    return;
                }
                it->second.kind = act.destination;
                it->second.consignee = CitizenId::None;
                it->second.consignee_business = BusinessId::None;
                it->second.consignment_portion.clear();
                it->second.consignment_terms.clear();
                if (act.destination != PoolKind::Commons) {
                    it->second.holder = CitizenId::None;
                    it->second.bond = 0;
                }
            } else if constexpr (std::is_same_v<T, ClaimVacantParcel>) {
                auto it = parcels_.find(raw(act.parcel));
                if (it == parcels_.end()) {
                    err = ApplyError{"no such parcel"};
                    return;
                }
                if (it->second.holder != CitizenId::None) {
                    err = ApplyError{"parcel is not vacant"};
                    return;
                }
                if (it->second.kind != PoolKind::CitizenAllocable) {
                    err = ApplyError{"not citizen-allocable"};
                    return;
                }
                if (!known_active(act.claimant)) {
                    err = ApplyError{"claimant not active"};
                    return;
                }
                it->second.holder = act.claimant;
            } else if constexpr (std::is_same_v<T, OpenBusiness>) {
                if (act.id == BusinessId::None || act.handle.empty()) {
                    err = ApplyError{"invalid business"};
                    return;
                }
                if (!known_active(act.opener)) {
                    err = ApplyError{"opener not active"};
                    return;
                }
                if (businesses_.count(raw(act.id))) {
                    err = ApplyError{"business exists"};
                    return;
                }
                businesses_[raw(act.id)] = Business{act.id, act.handle, {act.opener}};
            } else if constexpr (std::is_same_v<T, JoinBusiness>) {
                auto it = businesses_.find(raw(act.business));
                if (it == businesses_.end()) {
                    err = ApplyError{"no such business"};
                    return;
                }
                if (!known_active(act.joiner)) {
                    err = ApplyError{"joiner not active"};
                    return;
                }
                for (auto c : it->second.controllers) {
                    if (c == act.joiner) {
                        err = ApplyError{"already a controller"};
                        return;
                    }
                }
                it->second.controllers.push_back(act.joiner);
            } else if constexpr (std::is_same_v<T, AssertGenesisAuthority>) {
                if (!has_standing(act.holder)) {
                    err = ApplyError{"genesis holder has no standing"};
                    return;
                }
                if (genesis_holder_ != IdentityId::None) {
                    err = ApplyError{"genesis authority already asserted"};
                    return;
                }
                genesis_holder_ = act.holder;
            } else if constexpr (std::is_same_v<T, RecordJudgment>) {
                if (act.id == JudgmentId::None || act.subject == IdentityId::None || act.law.empty()) {
                    err = ApplyError{"invalid judgment"};
                    return;
                }
                if (judgments_.count(raw(act.id))) {
                    err = ApplyError{"judgment exists"};
                    return;
                }
                if (!has_standing(act.subject)) {
                    err = ApplyError{"subject has no standing"};
                    return;
                }
                if (act.harmed != IdentityId::None && !has_standing(act.harmed)) {
                    err = ApplyError{"harmed party has no standing"};
                    return;
                }
                if (act.author != JudgmentAuthor::Genesis) {
                    err = ApplyError{"only genesis author is live"};
                    return;
                }
                if (genesis_holder_ == IdentityId::None || act.author_holder != genesis_holder_) {
                    err = ApplyError{"not the genesis authority"};
                    return;
                }
                judgments_[raw(act.id)] = Judgment{act.id, act.subject, act.harmed, act.author,
                                                   act.author_holder, act.law, act.point_on_range,
                                                   act.reasons, ""};
            } else if constexpr (std::is_same_v<T, NoteJudgmentOutcome>) {
                auto it = judgments_.find(raw(act.judgment));
                if (it == judgments_.end()) {
                    err = ApplyError{"no such judgment"};
                    return;
                }
                if (act.outcome.empty()) {
                    err = ApplyError{"empty outcome"};
                    return;
                }
                if (act.by != it->second.author_holder) {
                    err = ApplyError{"only the issuing author may note outcome"};
                    return;
                }
                it->second.outcome = act.outcome;
            } else if constexpr (std::is_same_v<T, RenounceGenesisAuthority>) {
                if (genesis_holder_ == IdentityId::None || act.by != genesis_holder_) {
                    err = ApplyError{"not the genesis authority"};
                    return;
                }
                genesis_holder_ = IdentityId::None;
            }
        },
        ev.act);

    if (err) return err;

    ledger_.push_back(preview);
    last_event_ = preview.id;
    last_ts_ = preview.at;
    last_hash_ = preview.content_hash;
    return std::nullopt;
}

} // namespace liquidgov
