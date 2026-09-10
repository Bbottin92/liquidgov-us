#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace liquidgov {

// Strong identifiers keep civic objects from being mixed in tallies and the ledger.
enum class IdentityId : std::uint64_t { None = 0 };
enum class CitizenId : std::uint64_t { None = 0 };

inline CitizenId as_citizen(IdentityId id) {
    return static_cast<CitizenId>(static_cast<std::uint64_t>(id));
}
inline IdentityId as_identity(CitizenId id) {
    return static_cast<IdentityId>(static_cast<std::uint64_t>(id));
}
enum class MotionId : std::uint64_t { None = 0 };
enum class SeatId : std::uint64_t { None = 0 };
enum class ParcelId : std::uint64_t { None = 0 };
enum class BusinessId : std::uint64_t { None = 0 };
enum class PoolKind : std::uint8_t {
    CitizenAllocable = 0,
    ReservedNature = 1,
    Infrastructure = 2,
    Commons = 3
};
enum class OfficeId : std::uint64_t { None = 0 };
enum class EventId : std::uint64_t { None = 0 };
enum class TopicId : std::uint64_t { All = 0 };
enum class JudgmentId : std::uint64_t { None = 0 };
enum class JudgmentAuthor : std::uint8_t { Judge = 0, Polity = 1, Panel = 2, Genesis = 3 };
// 0 is the national scope, not “none.” Formation of other localities is unbuilt.
enum class LocalityId : std::uint64_t { National = 0 };

using Weight = std::uint64_t;
using Timestamp = std::uint64_t; // unix seconds; wall clock is an input, not a hidden global

inline constexpr Timestamp kSecondsPerYear = 365ull * 24ull * 3600ull;
// 0 = ceiling unset (§13 first-vote item). When set, a delegate may not
// hold more than this many vote points on a topic.
inline constexpr Weight kUnsetDelegateCeiling = 0;

inline constexpr std::string_view kThreeLaws =
    "Do not steal. Do not damage. Do not deprive of liberty.";

} // namespace liquidgov
