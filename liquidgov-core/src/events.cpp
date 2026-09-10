#include "liquidgov/events.hpp"

#include <cstring>
#include <sstream>
#include <type_traits>

namespace liquidgov {
namespace {

void put_u64(std::ostringstream& o, std::uint64_t v) {
    o << v << '\n';
}

void put_str(std::ostringstream& o, std::string_view s) {
    o << s.size() << ':' << s << '\n';
}

void encode_act(std::ostringstream& o, const CivicAct& act) {
    o << act.index() << '\n';
    std::visit([&](const auto& a) {
        using T = std::decay_t<decltype(a)>;
        if constexpr (std::is_same_v<T, EnrollCitizen>) {
            put_u64(o, static_cast<std::uint64_t>(a.id));
            put_str(o, a.handle);
        } else if constexpr (std::is_same_v<T, ResignCitizen>) {
            put_u64(o, static_cast<std::uint64_t>(a.id));
        } else if constexpr (std::is_same_v<T, SetDelegation>) {
            put_u64(o, static_cast<std::uint64_t>(a.from));
            put_u64(o, static_cast<std::uint64_t>(a.to));
            put_u64(o, static_cast<std::uint64_t>(a.topic));
        } else if constexpr (std::is_same_v<T, FileMotion>) {
            put_u64(o, static_cast<std::uint64_t>(a.id));
            put_u64(o, static_cast<std::uint64_t>(a.author));
            put_u64(o, static_cast<std::uint64_t>(a.topic));
            put_str(o, a.title);
            put_str(o, a.body);
        } else if constexpr (std::is_same_v<T, CastBallot>) {
            put_u64(o, static_cast<std::uint64_t>(a.motion));
            put_u64(o, static_cast<std::uint64_t>(a.voter));
            put_u64(o, static_cast<std::uint64_t>(a.choice));
        } else if constexpr (std::is_same_v<T, OpenSeat>) {
            put_u64(o, static_cast<std::uint64_t>(a.id));
            put_str(o, a.name);
            put_u64(o, static_cast<std::uint64_t>(a.topic));
        } else if constexpr (std::is_same_v<T, ClaimSeat>) {
            put_u64(o, static_cast<std::uint64_t>(a.seat));
            put_u64(o, static_cast<std::uint64_t>(a.claimant));
        } else if constexpr (std::is_same_v<T, VacateSeat>) {
            put_u64(o, static_cast<std::uint64_t>(a.seat));
            put_u64(o, static_cast<std::uint64_t>(a.by));
        } else if constexpr (std::is_same_v<T, RecordParcel>) {
            put_u64(o, static_cast<std::uint64_t>(a.id));
            put_u64(o, static_cast<std::uint64_t>(a.holder));
            put_str(o, a.description);
            if (a.kind != PoolKind::CitizenAllocable || a.bond != 0) {
                put_u64(o, static_cast<std::uint64_t>(a.kind));
                put_u64(o, a.bond);
            }
        } else if constexpr (std::is_same_v<T, TransferParcel>) {
            put_u64(o, static_cast<std::uint64_t>(a.id));
            put_u64(o, static_cast<std::uint64_t>(a.from));
            put_u64(o, static_cast<std::uint64_t>(a.to));
        } else if constexpr (std::is_same_v<T, OpenOffice>) {
            put_u64(o, static_cast<std::uint64_t>(a.id));
            put_str(o, a.name);
        } else if constexpr (std::is_same_v<T, AppointOffice>) {
            put_u64(o, static_cast<std::uint64_t>(a.office));
            put_u64(o, static_cast<std::uint64_t>(a.appointee));
        } else if constexpr (std::is_same_v<T, RemoveOffice>) {
            put_u64(o, static_cast<std::uint64_t>(a.office));
            put_u64(o, static_cast<std::uint64_t>(a.by));
        } else if constexpr (std::is_same_v<T, BindContract>) {
            put_u64(o, static_cast<std::uint64_t>(a.office));
            put_u64(o, static_cast<std::uint64_t>(a.official));
            put_u64(o, static_cast<std::uint64_t>(a.topic));
        } else if constexpr (std::is_same_v<T, NoteAlignment>) {
            put_u64(o, static_cast<std::uint64_t>(a.office));
            put_u64(o, static_cast<std::uint64_t>(a.motion));
            put_u64(o, a.followed ? 1 : 0);
            put_str(o, a.explanation);
        } else if constexpr (std::is_same_v<T, EnrollIdentity>) {
            put_u64(o, static_cast<std::uint64_t>(a.id));
            put_str(o, a.handle);
        } else if constexpr (std::is_same_v<T, GrantCitizenship>) {
            put_u64(o, static_cast<std::uint64_t>(a.id));
        } else if constexpr (std::is_same_v<T, ConsignParcel>) {
            put_u64(o, static_cast<std::uint64_t>(a.parcel));
            put_u64(o, static_cast<std::uint64_t>(a.from));
            put_u64(o, static_cast<std::uint64_t>(a.to));
            put_u64(o, static_cast<std::uint64_t>(a.to_business));
            put_str(o, a.portion);
            put_str(o, a.terms);
        } else if constexpr (std::is_same_v<T, ReleaseConsignment>) {
            put_u64(o, static_cast<std::uint64_t>(a.parcel));
            put_u64(o, static_cast<std::uint64_t>(a.by));
        } else if constexpr (std::is_same_v<T, DedicateParcel>) {
            put_u64(o, static_cast<std::uint64_t>(a.parcel));
            put_u64(o, static_cast<std::uint64_t>(a.by));
            put_u64(o, static_cast<std::uint64_t>(a.destination));
        } else if constexpr (std::is_same_v<T, ClaimVacantParcel>) {
            put_u64(o, static_cast<std::uint64_t>(a.parcel));
            put_u64(o, static_cast<std::uint64_t>(a.claimant));
        } else if constexpr (std::is_same_v<T, OpenBusiness>) {
            put_u64(o, static_cast<std::uint64_t>(a.id));
            put_u64(o, static_cast<std::uint64_t>(a.opener));
            put_str(o, a.handle);
        } else if constexpr (std::is_same_v<T, JoinBusiness>) {
            put_u64(o, static_cast<std::uint64_t>(a.business));
            put_u64(o, static_cast<std::uint64_t>(a.joiner));
        } else if constexpr (std::is_same_v<T, AssertGenesisAuthority>) {
            put_u64(o, static_cast<std::uint64_t>(a.holder));
        } else if constexpr (std::is_same_v<T, RecordJudgment>) {
            put_u64(o, static_cast<std::uint64_t>(a.id));
            put_u64(o, static_cast<std::uint64_t>(a.subject));
            put_u64(o, static_cast<std::uint64_t>(a.harmed));
            put_u64(o, static_cast<std::uint64_t>(a.author));
            put_u64(o, static_cast<std::uint64_t>(a.author_holder));
            put_str(o, a.law);
            put_str(o, a.point_on_range);
            put_str(o, a.reasons);
        } else if constexpr (std::is_same_v<T, NoteJudgmentOutcome>) {
            put_u64(o, static_cast<std::uint64_t>(a.judgment));
            put_u64(o, static_cast<std::uint64_t>(a.by));
            put_str(o, a.outcome);
        } else if constexpr (std::is_same_v<T, RenounceGenesisAuthority>) {
            put_u64(o, static_cast<std::uint64_t>(a.by));
        }
    }, act);
}

} // namespace

std::string encode_for_hash(const LedgerEvent& ev) {
    std::ostringstream o;
    put_u64(o, static_cast<std::uint64_t>(ev.id));
    put_u64(o, ev.at);
    o.write(reinterpret_cast<const char*>(ev.prev_hash.data()), ev.prev_hash.size());
    o << '\n';
    encode_act(o, ev.act);
    return o.str();
}

Hash hash_event(const LedgerEvent& ev) {
    auto bytes = encode_for_hash(ev);
    return sha256(bytes);
}

namespace {

bool read_line_u64(std::string_view& in, std::uint64_t& out) {
    auto nl = in.find('\n');
    if (nl == std::string_view::npos) return false;
    auto line = in.substr(0, nl);
    if (line.empty()) return false;
    std::uint64_t v = 0;
    for (char c : line) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + static_cast<std::uint64_t>(c - '0');
    }
    out = v;
    in.remove_prefix(nl + 1);
    return true;
}

bool read_str(std::string_view& in, std::string& out) {
    auto colon = in.find(':');
    if (colon == std::string_view::npos) return false;
    std::uint64_t n = 0;
    auto num = in.substr(0, colon);
    if (num.empty()) return false;
    for (char c : num) {
        if (c < '0' || c > '9') return false;
        n = n * 10 + static_cast<std::uint64_t>(c - '0');
    }
    in.remove_prefix(colon + 1);
    if (in.size() < n + 1 || in[n] != '\n') return false;
    out.assign(in.data(), n);
    in.remove_prefix(n + 1);
    return true;
}

} // namespace

std::optional<LedgerEvent> decode_for_hash(std::string_view bytes) {
    LedgerEvent ev;
    std::uint64_t n = 0;
    if (!read_line_u64(bytes, n)) return std::nullopt;
    ev.id = EventId{n};
    if (!read_line_u64(bytes, n)) return std::nullopt;
    ev.at = n;
    if (bytes.size() < 33 || bytes[32] != '\n') return std::nullopt;
    std::memcpy(ev.prev_hash.data(), bytes.data(), 32);
    bytes.remove_prefix(33);

    if (!read_line_u64(bytes, n)) return std::nullopt;
    const auto idx = n;

    auto u64_id = [&](auto& dst) -> bool {
        std::uint64_t v = 0;
        if (!read_line_u64(bytes, v)) return false;
        dst = static_cast<std::decay_t<decltype(dst)>>(v);
        return true;
    };

    switch (idx) {
    case 0: {
        EnrollCitizen a{};
        if (!u64_id(a.id) || !read_str(bytes, a.handle)) return std::nullopt;
        ev.act = std::move(a);
        break;
    }
    case 1: {
        ResignCitizen a{};
        if (!u64_id(a.id)) return std::nullopt;
        ev.act = a;
        break;
    }
    case 2: {
        SetDelegation a{};
        if (!u64_id(a.from) || !u64_id(a.to) || !u64_id(a.topic)) return std::nullopt;
        ev.act = a;
        break;
    }
    case 3: {
        FileMotion a{};
        if (!u64_id(a.id) || !u64_id(a.author) || !u64_id(a.topic) ||
            !read_str(bytes, a.title) || !read_str(bytes, a.body))
            return std::nullopt;
        ev.act = std::move(a);
        break;
    }
    case 4: {
        CastBallot a{};
        std::uint64_t choice = 0;
        if (!u64_id(a.motion) || !u64_id(a.voter) || !read_line_u64(bytes, choice))
            return std::nullopt;
        a.choice = static_cast<CastBallot::Choice>(choice);
        ev.act = a;
        break;
    }
    case 5: {
        OpenSeat a{};
        if (!u64_id(a.id) || !read_str(bytes, a.name) || !u64_id(a.topic)) return std::nullopt;
        ev.act = std::move(a);
        break;
    }
    case 6: {
        ClaimSeat a{};
        if (!u64_id(a.seat) || !u64_id(a.claimant)) return std::nullopt;
        ev.act = a;
        break;
    }
    case 7: {
        VacateSeat a{};
        if (!u64_id(a.seat) || !u64_id(a.by)) return std::nullopt;
        ev.act = a;
        break;
    }
    case 8: {
        RecordParcel a{};
        if (!u64_id(a.id) || !u64_id(a.holder) || !read_str(bytes, a.description))
            return std::nullopt;
        if (!bytes.empty()) {
            std::uint64_t kind = 0;
            if (!read_line_u64(bytes, kind) || !read_line_u64(bytes, a.bond)) return std::nullopt;
            a.kind = static_cast<PoolKind>(kind);
        }
        ev.act = std::move(a);
        break;
    }
    case 9: {
        TransferParcel a{};
        if (!u64_id(a.id) || !u64_id(a.from) || !u64_id(a.to)) return std::nullopt;
        ev.act = a;
        break;
    }
    case 10: {
        OpenOffice a{};
        if (!u64_id(a.id) || !read_str(bytes, a.name)) return std::nullopt;
        ev.act = std::move(a);
        break;
    }
    case 11: {
        AppointOffice a{};
        if (!u64_id(a.office) || !u64_id(a.appointee)) return std::nullopt;
        ev.act = a;
        break;
    }
    case 12: {
        RemoveOffice a{};
        if (!u64_id(a.office) || !u64_id(a.by)) return std::nullopt;
        ev.act = a;
        break;
    }
    case 13: {
        BindContract a{};
        if (!u64_id(a.office) || !u64_id(a.official) || !u64_id(a.topic)) return std::nullopt;
        ev.act = a;
        break;
    }
    case 14: {
        NoteAlignment a{};
        std::uint64_t followed = 0;
        if (!u64_id(a.office) || !u64_id(a.motion) || !read_line_u64(bytes, followed) ||
            !read_str(bytes, a.explanation))
            return std::nullopt;
        a.followed = followed != 0;
        ev.act = std::move(a);
        break;
    }
    case 15: {
        EnrollIdentity a{};
        if (!u64_id(a.id) || !read_str(bytes, a.handle)) return std::nullopt;
        ev.act = std::move(a);
        break;
    }
    case 16: {
        GrantCitizenship a{};
        if (!u64_id(a.id)) return std::nullopt;
        ev.act = a;
        break;
    }
    case 17: {
        ConsignParcel a{};
        if (!u64_id(a.parcel) || !u64_id(a.from) || !u64_id(a.to) || !u64_id(a.to_business) ||
            !read_str(bytes, a.portion) || !read_str(bytes, a.terms))
            return std::nullopt;
        ev.act = std::move(a);
        break;
    }
    case 18: {
        ReleaseConsignment a{};
        if (!u64_id(a.parcel) || !u64_id(a.by)) return std::nullopt;
        ev.act = a;
        break;
    }
    case 19: {
        DedicateParcel a{};
        std::uint64_t dest = 0;
        if (!u64_id(a.parcel) || !u64_id(a.by) || !read_line_u64(bytes, dest)) return std::nullopt;
        a.destination = static_cast<PoolKind>(dest);
        ev.act = a;
        break;
    }
    case 20: {
        ClaimVacantParcel a{};
        if (!u64_id(a.parcel) || !u64_id(a.claimant)) return std::nullopt;
        ev.act = a;
        break;
    }
    case 21: {
        OpenBusiness a{};
        if (!u64_id(a.id) || !u64_id(a.opener) || !read_str(bytes, a.handle)) return std::nullopt;
        ev.act = std::move(a);
        break;
    }
    case 22: {
        JoinBusiness a{};
        if (!u64_id(a.business) || !u64_id(a.joiner)) return std::nullopt;
        ev.act = a;
        break;
    }
    case 23: {
        AssertGenesisAuthority a{};
        if (!u64_id(a.holder)) return std::nullopt;
        ev.act = a;
        break;
    }
    case 24: {
        RecordJudgment a{};
        std::uint64_t author = 0;
        if (!u64_id(a.id) || !u64_id(a.subject) || !u64_id(a.harmed) || !read_line_u64(bytes, author) ||
            !u64_id(a.author_holder) || !read_str(bytes, a.law) || !read_str(bytes, a.point_on_range) ||
            !read_str(bytes, a.reasons))
            return std::nullopt;
        a.author = static_cast<JudgmentAuthor>(author);
        ev.act = std::move(a);
        break;
    }
    case 25: {
        NoteJudgmentOutcome a{};
        if (!u64_id(a.judgment) || !u64_id(a.by) || !read_str(bytes, a.outcome)) return std::nullopt;
        ev.act = std::move(a);
        break;
    }
    case 26: {
        RenounceGenesisAuthority a{};
        if (!u64_id(a.by)) return std::nullopt;
        ev.act = a;
        break;
    }
    default:
        return std::nullopt;
    }
    if (!bytes.empty()) return std::nullopt;
    return ev;
}

} // namespace liquidgov
