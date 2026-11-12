// proposals: Proposals, their ballots, offices, cities, tags and votes.
// LGP1 record types: prop propn pvote pbk pbo pbv popen pba poff pcity pwho ptag (format: the header comment of file_store.hpp).
// Split out of file_store.cpp 2026-11-12 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"
#include "liquidgov/platform/clock.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

namespace {

bool valid_iso_date(const std::string& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (int i : {0, 1, 2, 3, 5, 6, 8, 9}) {
        if (s[static_cast<std::size_t>(i)] < '0' || s[static_cast<std::size_t>(i)] > '9') return false;
    }
    const int y = std::stoi(s.substr(0, 4));
    const int m = std::stoi(s.substr(5, 2));
    const int d = std::stoi(s.substr(8, 2));
    if (y < 2000 || y > 2100 || m < 1 || m > 12 || d < 1) return false;
    // a real calendar day (Brief 7 item 5: 2027-02-31 was accepted)
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return d <= days[m - 1] + ((m == 2 && leap) ? 1 : 0);
}

bool spec_ok(const BallotSpec& s) {
    const int k = static_cast<int>(s.kind);
    if (k < 0 || k > 4) return false;
    if (s.unit.size() > 24) return false;
    if (s.kind == BallotKind::Choice) {
        const std::size_t mn = s.open_choice ? 1 : 2;
        const std::size_t mx = s.open_choice ? 48 : 12;
        if (s.options.size() < mn || s.options.size() > mx) return false;
        for (const auto& o : s.options) {
            if (o.empty() || o.size() > 80) return false;
        }
        if (!ballot_options_unique(s.options)) return false;
    }
    if (s.kind == BallotKind::Scale) {
        if (s.options.size() > 2) return false;
        for (const auto& o : s.options) {
            if (o.size() > 40) return false;
        }
    }
    if (s.kind == BallotKind::Number) {
        if (s.min_v != 0 || s.max_v != 0) {
            if (!(s.min_v < s.max_v) || !std::isfinite(s.min_v) || !std::isfinite(s.max_v))
                return false;
        }
    }
    return true;
}

bool payload_ok(const BallotSpec& s, const std::string& raw) {
    if (raw.empty()) return false;
    if (raw == kBallotSkip) return s.kind != BallotKind::YesNo;
    if (raw == kBallotNone) return s.kind == BallotKind::Choice;
    if (s.kind == BallotKind::Number) {
        std::size_t idx = 0;
        double v = 0;
        try { v = std::stod(raw, &idx); } catch (...) { return false; }
        if (idx != raw.size() || !std::isfinite(v) || std::fabs(v) > 1e12) return false;
        if (s.min_v != 0 || s.max_v != 0) {
            if (v < s.min_v || v > s.max_v) return false;
        }
        return true;
    }
    if (s.kind == BallotKind::Date) return valid_iso_date(raw);
    if (s.kind == BallotKind::Choice) {
        std::size_t idx = 0;
        unsigned long i = 0;
        try { i = std::stoul(raw, &idx); } catch (...) { return false; }
        return idx == raw.size() && i < s.options.size();
    }
    if (s.kind == BallotKind::Scale) {
        std::size_t idx = 0;
        long i = 0;
        try { i = std::stol(raw, &idx); } catch (...) { return false; }
        return idx == raw.size() && i >= 0 && i <= 100;
    }
    return false;
}

std::string trim_label(std::string s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

} // namespace

bool ballot_options_unique(const std::vector<std::string>& options) {
    std::set<std::string> seen;
    for (auto o : options) {
        for (auto& ch : o) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        if (!seen.insert(o).second) return false;
    }
    return true;
}

std::uint64_t FileStore::add_proposal(const std::string& author, const std::string& title, const std::string& body,
                                      BallotSpec spec) {
    if (!spec_ok(spec)) return 0;
    Proposal p;
    p.id = next_proposal_id_++;
    p.author = author;
    p.ts = now();
    p.title = title;
    p.body = body;
    p.ballot = spec;
    proposals_.push_back(p);
    flush();
    return p.id;
}

bool FileStore::add_proposal_option(std::uint64_t proposal_id, const std::string& username, const std::string& label) {
    if (username.empty()) return false;
    const std::string lab = trim_label(label);
    if (lab.empty() || lab.size() > 80) return false;
    Proposal* p = nullptr;
    for (auto& it : proposals_) {
        if (it.id == proposal_id) { p = &it; break; }
    }
    if (!p || p->ballot.kind != BallotKind::Choice) return false;
    if (!p->ballot.open_choice && username != p->author) return false;
    if (p->ballot.options.size() >= 48) return false;
    if (p->ballot.option_authors.size() != p->ballot.options.size())
        p->ballot.option_authors.resize(p->ballot.options.size());
    const std::string want = lower_copy(lab);
    for (const auto& o : p->ballot.options) {
        if (lower_copy(o) == want) return false;
    }
    if (username != p->author) {
        int n = 0;
        for (const auto& a : p->ballot.option_authors) if (a == username) ++n;
        if (n >= 1) return false;
    }
    p->ballot.options.push_back(lab);
    p->ballot.option_authors.push_back(username == p->author ? std::string() : username);
    flush();
    return true;
}

bool FileStore::mark_proposal_office(std::uint64_t proposal_id, const std::string& jurisdiction) {
    if (jurisdiction.empty() || jurisdiction.size() > 80) return false;
    for (auto& p : proposals_) {
        if (p.id != proposal_id) continue;
        p.is_office = true;
        p.jurisdiction = jurisdiction;
        flush();
        return true;
    }
    return false;
}

bool FileStore::set_proposal_city(std::uint64_t proposal_id, const std::string& city) {
    Proposal* found = nullptr;
    for (auto& p : proposals_) {
        if (p.id == proposal_id) { found = &p; break; }
    }
    if (!found) return false;
    const std::string key = city_key(clean_city(city));
    if (key.empty()) {
        if (found->city.empty() && found->tags.empty()) return true;
        found->city.clear();
        found->tags.clear();
        flush();
        return true;
    }
    if (communities_in_city(key).empty()) return false;
    if (found->city == key) return true;
    found->city = key;
    found->tags.clear();
    flush();
    return true;
}

bool FileStore::set_proposal_actor(std::uint64_t proposal_id, const std::string& who) {
    Proposal* found = nullptr;
    for (auto& p : proposals_) {
        if (p.id == proposal_id) { found = &p; break; }
    }
    if (!found) return false;
    const std::string cleaned = clean_city(who);
    if (found->who_acts == cleaned) return true;
    found->who_acts = cleaned;
    flush();
    return true;
}

bool FileStore::delete_proposal(std::uint64_t proposal_id) {
    auto it = std::find_if(proposals_.begin(), proposals_.end(),
                           [&](const Proposal& p) { return p.id == proposal_id; });
    if (it == proposals_.end()) return false;
    proposals_.erase(it);
    proposal_replies_.erase(std::remove_if(proposal_replies_.begin(), proposal_replies_.end(),
                                           [&](const ProposalReply& r) { return r.proposal_id == proposal_id; }),
                            proposal_replies_.end());
    proposal_reactions_.erase(proposal_id);
    proposal_votes_.erase(proposal_id);
    ballot_values_.erase(proposal_id);
    prune_pains(); // suggestions whose source proposal or comment is gone
    flush();
    return true;
}

bool FileStore::edit_proposal(std::uint64_t proposal_id, const std::string& title, const std::string& body) {
    if (title.empty()) return false;
    for (auto& p : proposals_) {
        if (p.id != proposal_id) continue;
        if (p.title == title && p.body == body) return true;
        p.title = title;
        p.body = body;
        flush();
        return true;
    }
    return false;
}

bool FileStore::set_proposal_tags(std::uint64_t proposal_id, const std::vector<std::string>& tags) {
    Proposal* found = nullptr;
    for (auto& p : proposals_) {
        if (p.id == proposal_id) { found = &p; break; }
    }
    if (!found) return false;
    std::vector<std::string> next;
    if (!found->city.empty()) {
        for (const auto& tag : city_tags(found->city)) {
            const std::string fold = lower_copy(tag);
            for (const auto& wanted : tags)
                if (lower_copy(clean_city(wanted)) == fold) { next.push_back(tag); break; }
        }
    }
    if (next == found->tags) return true;
    found->tags = std::move(next);
    flush();
    return true;
}

std::vector<Proposal> FileStore::proposals_in_city(const std::string& key) const {
    std::vector<Proposal> out;
    if (key.empty()) return out;
    for (const auto& p : proposals_)
        if (p.city == key) out.push_back(p);
    return out;
}

std::vector<Proposal> FileStore::all_proposals() const {
    return proposals_;
}

std::optional<Proposal> FileStore::proposal_by_id(std::uint64_t id) const {
    for (const auto& p : proposals_) {
        if (p.id == id) return p;
    }
    return std::nullopt;
}

void FileStore::cast_proposal_vote(std::uint64_t proposal_id, const std::string& voter, ProposalVote choice) {
    if (voter.empty()) return;
    auto p = proposal_by_id(proposal_id);
    if (p && p->ballot.kind != BallotKind::YesNo) return;
    proposal_votes_[proposal_id][voter] = choice;
    flush();
}

std::optional<ProposalVote> FileStore::proposal_vote_of(std::uint64_t proposal_id, const std::string& voter) const {
    auto pit = proposal_votes_.find(proposal_id);
    if (pit == proposal_votes_.end()) return std::nullopt;
    auto vit = pit->second.find(voter);
    if (vit == pit->second.end()) return std::nullopt;
    return vit->second;
}

ProposalTally FileStore::tally_of(std::uint64_t proposal_id) const {
    ProposalTally t;
    auto pit = proposal_votes_.find(proposal_id);
    if (pit == proposal_votes_.end()) return t;
    for (const auto& [voter, choice] : pit->second) {
        switch (choice) {
            case ProposalVote::Yes: ++t.yes; break;
            case ProposalVote::No: ++t.no; break;
            case ProposalVote::Abstain: ++t.abstain; break;
        }
    }
    return t;
}

bool FileStore::cast_ballot_value(std::uint64_t proposal_id, const std::string& voter, const std::string& payload) {
    if (voter.empty()) return false;
    auto p = proposal_by_id(proposal_id);
    if (!p || p->ballot.kind == BallotKind::YesNo) return false;
    if (!payload_ok(p->ballot, payload)) return false;
    ballot_values_[proposal_id][voter] = payload;
    flush();
    return true;
}

std::optional<std::string> FileStore::ballot_value_of(std::uint64_t proposal_id, const std::string& voter) const {
    auto pit = ballot_values_.find(proposal_id);
    if (pit == ballot_values_.end()) return std::nullopt;
    auto vit = pit->second.find(voter);
    if (vit == pit->second.end()) return std::nullopt;
    return vit->second;
}

BallotTally FileStore::value_tally_of(std::uint64_t proposal_id) const {
    BallotTally t;
    auto p = proposal_by_id(proposal_id);
    if (!p) return t;
    auto pit = ballot_values_.find(proposal_id);
    if (pit == ballot_values_.end() || pit->second.empty()) {
        if (p->ballot.kind == BallotKind::Choice)
            t.option_counts.assign(p->ballot.options.size(), 0);
        return t;
    }
    t.n = 0;
    for (const auto& [voter, payload] : pit->second) if (payload == kBallotSkip) ++t.skipped;
    if (p->ballot.kind == BallotKind::Choice) {
        t.option_counts.assign(p->ballot.options.size(), 0);
        for (const auto& [voter, payload] : pit->second) {
            if (payload == kBallotSkip) continue;
            if (payload == kBallotNone) { ++t.none_of_these; ++t.n; continue; }
            std::size_t idx = 0;
            unsigned long i = 0;
            try { i = std::stoul(payload, &idx); } catch (...) { continue; }
            if (idx == payload.size() && i < t.option_counts.size()) {
                ++t.option_counts[i];
                ++t.n;
            }
        }
        return t;
    }
    if (p->ballot.kind == BallotKind::Date) {
        std::vector<std::string> dates;
        dates.reserve(pit->second.size());
        for (const auto& [voter, payload] : pit->second)
            if (payload != kBallotSkip) dates.push_back(payload);
        if (dates.empty()) return t;
        t.n = static_cast<int>(dates.size());
        std::sort(dates.begin(), dates.end());
        t.lo_date = dates.front();
        t.hi_date = dates.back();
        t.median_date = dates[(dates.size() - 1) / 2];
        return t;
    }
    std::vector<double> vals;
    vals.reserve(pit->second.size());
    for (const auto& [voter, payload] : pit->second) {
        if (payload == kBallotSkip) continue;
        try { vals.push_back(std::stod(payload)); } catch (...) {}
    }
    if (vals.empty()) return t;
    std::sort(vals.begin(), vals.end());
    t.lo = vals.front();
    t.hi = vals.back();
    t.median = vals[(vals.size() - 1) / 2];
    t.n = static_cast<int>(vals.size());
    return t;
}

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_proposals(const std::vector<std::string>& f, LoadPending& pending) {
    using PendingBk = LoadPending::PendingBk;
    auto& pending_bk = pending.pending_bk;
    auto& pending_bo = pending.pending_bo;
    auto& pending_pba = pending.pending_pba;
    auto& pending_pcity = pending.pending_pcity;
    auto& pending_poff = pending.pending_poff;
    auto& pending_popen = pending.pending_popen;
    auto& pending_ptag = pending.pending_ptag;
    auto& pending_pwho = pending.pending_pwho;
    const std::string& tag = f[0];
    if (tag == "prop") {
        if (f.size() != 6) throw std::runtime_error("LGP1: bad prop record");
        Proposal p;
        p.id = parse_u64(f[1], "proposal id");
        p.author = pct_decode(f[2]);
        p.ts = parse_i64(f[3], "proposal ts");
        p.title = pct_decode(f[4]);
        p.body = pct_decode(f[5]);
        proposals_.push_back(p);
        if (p.id >= next_proposal_id_) next_proposal_id_ = p.id + 1;
        return true;
    }
    if (tag == "propn") {
        if (f.size() != 2) throw std::runtime_error("LGP1: bad propn record");
        next_proposal_id_ = parse_u64(f[1], "next_proposal_id");
        if (next_proposal_id_ == 0) next_proposal_id_ = 1;
        return true;
    }
    if (tag == "pvote") {
        if (f.size() != 4) throw std::runtime_error("LGP1: bad pvote record");
        const auto pid = parse_u64(f[1], "pvote proposal id");
        const auto voter = pct_decode(f[2]);
        const auto cv = parse_u64(f[3], "pvote choice");
        if (cv > 2) throw std::runtime_error("LGP1: bad pvote choice");
        proposal_votes_[pid][voter] = static_cast<ProposalVote>(cv);
        return true;
    }
    if (tag == "pbk") {
        if (f.size() != 7) throw std::runtime_error("LGP1: bad pbk record");
        const auto pid = parse_u64(f[1], "pbk proposal id");
        const auto kind = parse_u64(f[2], "pbk kind");
        if (kind < 1 || kind > 4) throw std::runtime_error("LGP1: bad pbk kind");
        pending_bk[pid] = PendingBk{
            static_cast<int>(kind), pct_decode(f[3]),
            parse_dbl(f[4], "pbk min"), parse_dbl(f[5], "pbk max"),
            parse_dbl(f[6], "pbk step")
        };
        return true;
    }
    if (tag == "pbo") {
        if (f.size() != 4) throw std::runtime_error("LGP1: bad pbo record");
        const auto pid = parse_u64(f[1], "pbo proposal id");
        const auto idx = parse_u64(f[2], "pbo index");
        pending_bo[pid].push_back({static_cast<int>(idx), pct_decode(f[3])});
        return true;
    }
    if (tag == "pbv") {
        if (f.size() != 4) throw std::runtime_error("LGP1: bad pbv record");
        const auto pid = parse_u64(f[1], "pbv proposal id");
        const auto voter = pct_decode(f[2]);
        if (!voter.empty()) ballot_values_[pid][voter] = pct_decode(f[3]);
        return true;
    }
    if (tag == "popen") {
        if (f.size() != 2) throw std::runtime_error("LGP1: bad popen record");
        pending_popen.insert(parse_u64(f[1], "popen proposal id"));
        return true;
    }
    if (tag == "pba") {
        if (f.size() != 4) throw std::runtime_error("LGP1: bad pba record");
        const auto pid = parse_u64(f[1], "pba proposal id");
        const auto idx = parse_u64(f[2], "pba index");
        const auto who = pct_decode(f[3]);
        if (!who.empty()) pending_pba[pid].push_back({static_cast<int>(idx), who});
        return true;
    }
    if (tag == "poff") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad poff record");
        pending_poff[parse_u64(f[1], "poff proposal id")] = pct_decode(f[2]);
        return true;
    }
    if (tag == "pcity") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad pcity record");
        pending_pcity[parse_u64(f[1], "pcity proposal id")] = pct_decode(f[2]);
        return true;
    }
    if (tag == "pwho") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad pwho record");
        pending_pwho[parse_u64(f[1], "pwho proposal id")] = pct_decode(f[2]);
        return true;
    }
    if (tag == "ptag") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad ptag record");
        pending_ptag[parse_u64(f[1], "ptag proposal id")].push_back(pct_decode(f[2]));
        return true;
    }
    return false;
}

// After every line is read (load() calls the finishers in a fixed order).
void FileStore::finish_proposals(LoadPending& pending) {
    auto& pending_bk = pending.pending_bk;
    auto& pending_bo = pending.pending_bo;
    auto& pending_pba = pending.pending_pba;
    auto& pending_pcity = pending.pending_pcity;
    auto& pending_poff = pending.pending_poff;
    auto& pending_popen = pending.pending_popen;
    auto& pending_ptag = pending.pending_ptag;
    auto& pending_pwho = pending.pending_pwho;
    for (auto& p : proposals_) {
        auto bit = pending_bk.find(p.id);
        if (bit != pending_bk.end()) {
            p.ballot.kind = static_cast<BallotKind>(bit->second.kind);
            p.ballot.unit = bit->second.unit;
            p.ballot.min_v = bit->second.min_v;
            p.ballot.max_v = bit->second.max_v;
            p.ballot.step = bit->second.step;
        }
        auto oit = pending_bo.find(p.id);
        if (oit != pending_bo.end()) {
            auto opts = oit->second;
            std::sort(opts.begin(), opts.end());
            p.ballot.options.clear();
            p.ballot.option_authors.clear();
            for (const auto& item : opts) {
                p.ballot.options.push_back(item.second);
                p.ballot.option_authors.push_back("");
            }
        }
        if (pending_popen.count(p.id)) p.ballot.open_choice = true;
        auto off = pending_poff.find(p.id);
        if (off != pending_poff.end()) {
            p.is_office = true;
            p.jurisdiction = off->second;
        }
        auto pcit = pending_pcity.find(p.id);
        if (pcit != pending_pcity.end()) {
            const std::string key = city_key(clean_city(pcit->second));
            if (!key.empty()) p.city = key; // kept even while no group names it
        }
        auto pwit = pending_pwho.find(p.id);
        if (pwit != pending_pwho.end()) p.who_acts = clean_city(pwit->second);
        auto ptit = pending_ptag.find(p.id);
        if (ptit != pending_ptag.end() && !p.city.empty()) {
            for (const auto& tag : city_tags(p.city)) {
                const std::string fold = lower_copy(tag);
                for (const auto& wanted : ptit->second)
                    if (lower_copy(clean_city(wanted)) == fold) { p.tags.push_back(tag); break; }
            }
        }
        auto ait = pending_pba.find(p.id);
        if (ait != pending_pba.end()) {
            if (p.ballot.option_authors.size() != p.ballot.options.size())
                p.ballot.option_authors.resize(p.ballot.options.size());
            for (const auto& item : ait->second) {
                if (item.first >= 0 && static_cast<std::size_t>(item.first) < p.ballot.option_authors.size())
                    p.ballot.option_authors[static_cast<std::size_t>(item.first)] = item.second;
            }
        }
    }
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_proposals(std::ostream& body) {
    // A city's tags and its proposals' city stay even while no group names
    // that city; they show again once a group does (Brandon 2026-11-06).
    for (auto& p : proposals_) {
        if (p.city.empty()) { p.tags.clear(); continue; }
        std::vector<std::string> keep;
        auto tags = city_tags_.find(p.city);
        if (tags != city_tags_.end()) {
            for (const auto& tag : tags->second) {
                for (const auto& have : p.tags)
                    if (lower_copy(have) == lower_copy(tag)) { keep.push_back(tag); break; }
            }
        }
        p.tags = std::move(keep);
    }
    body << "propn\t" << next_proposal_id_ << "\n";
    for (const auto& p : proposals_) {
        body << "prop\t" << p.id << "\t" << pct_encode(p.author) << "\t" << p.ts << "\t"
             << pct_encode(p.title) << "\t" << pct_encode(p.body) << "\n";
        if (!p.city.empty()) body << "pcity\t" << p.id << "\t" << pct_encode(p.city) << "\n";
        if (!p.who_acts.empty()) body << "pwho\t" << p.id << "\t" << pct_encode(p.who_acts) << "\n";
        for (const auto& tag : p.tags)
            body << "ptag\t" << p.id << "\t" << pct_encode(tag) << "\n";
    }
    for (const auto& [pid, votes] : proposal_votes_) {
        for (const auto& [voter, choice] : votes) {
            body << "pvote\t" << pid << "\t" << pct_encode(voter) << "\t" << static_cast<int>(choice) << "\n";
        }
    }
    for (const auto& p : proposals_) {
        if (p.ballot.kind == BallotKind::YesNo) continue;
        body << "pbk\t" << p.id << "\t" << static_cast<int>(p.ballot.kind) << "\t"
             << pct_encode(p.ballot.unit) << "\t" << fmt_stored(p.ballot.min_v) << "\t"
             << fmt_stored(p.ballot.max_v) << "\t" << fmt_stored(p.ballot.step) << "\n";
        for (std::size_t i = 0; i < p.ballot.options.size(); ++i) {
            body << "pbo\t" << p.id << "\t" << i << "\t" << pct_encode(p.ballot.options[i]) << "\n";
        }
        if (p.ballot.open_choice) body << "popen\t" << p.id << "\n";
        if (p.is_office) body << "poff\t" << p.id << "\t" << pct_encode(p.jurisdiction) << "\n";
        if (p.ballot.option_authors.size() == p.ballot.options.size()) {
            for (std::size_t i = 0; i < p.ballot.option_authors.size(); ++i) {
                if (p.ballot.option_authors[i].empty()) continue;
                body << "pba\t" << p.id << "\t" << i << "\t" << pct_encode(p.ballot.option_authors[i]) << "\n";
            }
        }
    }
    for (const auto& [pid, vals] : ballot_values_) {
        for (const auto& [voter, payload] : vals) {
            body << "pbv\t" << pid << "\t" << pct_encode(voter) << "\t" << pct_encode(payload) << "\n";
        }
    }
}

} // namespace liquidgov::platform
