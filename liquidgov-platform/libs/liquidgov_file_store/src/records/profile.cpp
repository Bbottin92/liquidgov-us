// profile: Profile text, blurbs, author CSS, canvas frames.
// LGP1 record types: prof ac cvf cvm (format: the header comment of file_store.hpp).
// Split out of file_store.cpp 2026-11-12 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"

#include <algorithm>
#include <cstddef>
#include <ostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

void FileStore::set_blurb(AccountId id, const std::string& blurb) {
    blurbs_[id] = blurb;
    flush();
}

std::string FileStore::blurb(AccountId id) const {
    auto it = blurbs_.find(id);
    return it == blurbs_.end() ? "" : it->second;
}

void FileStore::set_profile(const std::string& username, const Profile& p) {
    if (!username_exists(username)) return;
    profiles_[username] = p;
    flush();
}

Profile FileStore::profile_of(const std::string& username) const {
    auto it = profiles_.find(username);
    return it == profiles_.end() ? Profile{} : it->second;
}

std::vector<std::string> FileStore::findable_sample(const std::string& requester, std::size_t n) const {
    std::vector<std::string> pool;
    for (const auto& [name, id] : by_username_) {
        (void)id;
        if (name == requester) continue;
        if (is_blocked(requester, name) || is_blocked(name, requester)) continue;
        if (are_friends(requester, name)) {
            pool.push_back(name);
            continue;
        }
        if (prefs_of(name).findability == Findability::Typeahead) pool.push_back(name);
    }
    if (pool.size() <= n) return pool;
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::shuffle(pool.begin(), pool.end(), rng);
    pool.resize(n);
    return pool;
}

void FileStore::set_author_css(AccountId id, const std::string& css) {
    if (css.empty()) author_css_.erase(id);
    else author_css_[id] = css;
    flush();
}

std::string FileStore::author_css(AccountId id) const {
    auto it = author_css_.find(id);
    return it == author_css_.end() ? "" : it->second;
}

void FileStore::clear_canvas_frames(AccountId id) {
    canvas_frames_.erase(id);
    flush();
}

void FileStore::add_canvas_frame(AccountId id, const std::string& blob_id) {
    canvas_frames_[id].push_back(blob_id);
    flush();
}

std::vector<std::string> FileStore::canvas_frames(AccountId id) const {
    auto it = canvas_frames_.find(id);
    return it == canvas_frames_.end() ? std::vector<std::string>{} : it->second;
}

void FileStore::set_canvas_timelapse(AccountId id, bool on) {
    canvas_timelapse_[id] = on;
    flush();
}

bool FileStore::canvas_timelapse(AccountId id) const {
    auto it = canvas_timelapse_.find(id);
    return it == canvas_timelapse_.end() ? false : it->second;
}

std::vector<std::string> FileStore::all_canvas_frame_blob_ids() const {
    std::vector<std::string> out;
    for (const auto& [id, frames] : canvas_frames_) {
        out.insert(out.end(), frames.begin(), frames.end());
    }
    return out;
}

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_profile(const std::vector<std::string>& f, LoadPending& pending) {
    (void)pending;
    const std::string& tag = f[0];
    if (tag == "prof") {
        if (f.size() != 9) throw std::runtime_error("LGP1: bad prof record");
        Profile p;
        const std::string name = pct_decode(f[1]);
        p.email_fp = pct_decode(f[2]);
        p.display_name = pct_decode(f[3]);
        p.hometown = pct_decode(f[4]);
        p.living = pct_decode(f[5]);
        p.interests = pct_decode(f[6]);
        p.career = pct_decode(f[7]);
        p.school = pct_decode(f[8]);
        if (!name.empty()) profiles_[name] = p;
        return true;
    }
    if (tag == "ac") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad ac record");
        AccountId id = parse_u64(f[1], "author_css account");
        if (id == 0) throw std::runtime_error("LGP1: empty ac record");
        author_css_[id] = pct_decode(f[2]);
        return true;
    }
    if (tag == "cvf") {
        if (f.size() != 4) throw std::runtime_error("LGP1: bad cvf record");
        AccountId id = parse_u64(f[1], "canvas frame account");
        int idx = static_cast<int>(parse_u64(f[2], "canvas frame index"));
        std::string blob_id = pct_decode(f[3]);
        if (id == 0 || blob_id.empty()) throw std::runtime_error("LGP1: empty cvf record");
        auto& frames = canvas_frames_[id];
        if (static_cast<int>(frames.size()) <= idx) frames.resize(idx + 1);
        frames[idx] = blob_id;
        return true;
    }
    if (tag == "cvm") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad cvm record");
        AccountId id = parse_u64(f[1], "canvas mode account");
        if (id == 0) throw std::runtime_error("LGP1: empty cvm record");
        canvas_timelapse_[id] = parse_bit(f[2], "canvas timelapse");
        return true;
    }
    return false;
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_profiles(std::ostream& body) {
    for (const auto& [name, p] : profiles_) {
        body << "prof\t" << pct_encode(name) << "\t" << pct_encode(p.email_fp) << "\t"
             << pct_encode(p.display_name) << "\t" << pct_encode(p.hometown) << "\t"
             << pct_encode(p.living) << "\t" << pct_encode(p.interests) << "\t"
             << pct_encode(p.career) << "\t" << pct_encode(p.school) << "\n";
    }
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_canvas(std::ostream& body) {
    for (const auto& [id, css] : author_css_) {
        if (!accounts_.count(id)) continue;
        if (css.empty()) continue;
        body << "ac\t" << id << "\t" << pct_encode(css) << "\n";
    }
    for (const auto& [id, frames] : canvas_frames_) {
        if (!accounts_.count(id)) continue;
        for (std::size_t i = 0; i < frames.size(); ++i) {
            if (frames[i].empty()) continue; // a resize() gap; shouldn't happen via add_canvas_frame
            body << "cvf\t" << id << "\t" << i << "\t" << pct_encode(frames[i]) << "\n";
        }
    }
    for (const auto& [id, on] : canvas_timelapse_) {
        if (!accounts_.count(id)) continue;
        body << "cvm\t" << id << "\t" << (on ? 1 : 0) << "\n";
    }
}

} // namespace liquidgov::platform
