#pragma once
// credibility.hpp: credibility, first slice (Brandon 2026-11-15: "a good, fair way to allow people to earn more
// credibility ... but i want to make sure that one person cant sabotage anothers credibility";
// Persistence/TheCommons/CLAUDE_CREDIBILITY_QUESTION.md). Contribution only, computed from what the site already keeps
// (nothing new is stored), and shown only to the person it is about (Settings), until Brandon decides how it shows.
#include "site/common.hpp"

namespace liquidgov::site {

struct Contribution {
    int proposals = 0, posts = 0;            // what you made
    int people = 0;                          // distinct others who replied to, voted on or reacted to it (each once)
    int beyond_friends = 0;                  // of those, people who are not your friends (they count double)
    std::int64_t days = 0;                   // days since you enrolled
    std::vector<std::pair<std::string, int>> groups;   // group name -> distinct people engaging with your posts there
    std::string level;                       // New, Emerging, Established, Trusted
};
Contribution contribution_of(const std::string& user);
std::string credibility_settings_card(const std::string& user);

}  // namespace liquidgov::site
