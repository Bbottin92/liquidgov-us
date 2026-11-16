#pragma once
// Outside content for interest views (Brandon 2026-11-16: "we need to figure out how to connect other websites into this
// system ... things could grow from wikipedia over time"). First connector: Wikipedia's page-summary service. An interest
// that has an article shows its short summary inside its pool, labelled "from Wikipedia". Rules: summary text only (never
// a copy of the article), cut short, cached, one quick request with a short time limit, a generic client name, and
// nothing about the visitor is sent. No article, a disambiguation page or any error means "nothing to show".
#include "site/common.hpp"
#include "site/features/map/map_rules.hpp"

namespace liquidgov::site {

// The summary of the article called `term` (title, plain-text extract cut to about 600 characters, article address), or
// false. Cached 24 h for a hit and 1 h for a miss, at most 500 entries. LIQUIDGOV_WIKI_BASE overrides the service
// address (the tests point it at a local stand-in; it ends in "/").
bool outside_wikipedia(const std::string& term, std::string& title, std::string& extract, std::string& url);

// For a pool of kind "interest": fill its body, by and sub from Wikipedia when there is a summary. Anything else is left
// as it is.
void outside_fill_interest(GNode& n);
// The same for every interest among the pools you are about to see (a person's interest boxes), all at once on their own
// threads so the whole list waits one request time, not one per interest; nothing is fetched twice (the cache). Called
// from /map/node only, never for search results.
void outside_fill_interests(std::vector<GLink>& links);

}  // namespace liquidgov::site
