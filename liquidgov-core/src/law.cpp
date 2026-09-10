#include "liquidgov/law.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace liquidgov {
namespace {

std::string lower(std::string_view s) {
    std::string o(s);
    std::transform(o.begin(), o.end(), o.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return o;
}

bool contains(const std::string& hay, std::string_view needle) {
    return hay.find(needle) != std::string::npos;
}

} // namespace

LawVerdict ThreeLaws::examine(std::string_view title, std::string_view body) {
    const std::string t = lower(title) + " " + lower(body);

    // Narrow, explicit gates. The point is an enforceable invariant in the core,
    // not a complete NLP classifier.
    static constexpr std::string_view steal[] = {
        "seize property", "confiscate", "expropriate", "take their land",
        "forfeit without", "steal "};
    for (auto p : steal) {
        if (contains(t, p)) {
            return {false, LawKind::Steal, "Motion authorizes taking what is held."};
        }
    }

    static constexpr std::string_view damage[] = {
        "destroy property", "burn ", "vandalize", "sabotage"};
    for (auto p : damage) {
        if (contains(t, p)) {
            return {false, LawKind::Damage, "Motion authorizes damage."};
        }
    }

    static constexpr std::string_view liberty[] = {
        "imprison without", "detain without", "compel association",
        "force membership", "enslave", "conscript "};
    for (auto p : liberty) {
        if (contains(t, p)) {
            return {false, LawKind::DepriveLiberty, "Motion authorizes deprivation of liberty."};
        }
    }

    return {};
}

} // namespace liquidgov
