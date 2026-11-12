// jukebox: Jukebox: queues, history, subscriptions, autoplay, relays, positions.
// LGP1 record types: jt jh jsub jauto jrelay jpos (format: the header comment of file_store.hpp).
// Split out of file_store.cpp 2026-11-12 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"
#include "liquidgov/platform/clock.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

namespace {

bool valid_youtube_id(const std::string& id) {
    if (id.size() != 11) return false;
    for (unsigned char c : id) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

std::string lower_ascii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

} // namespace

std::optional<std::string> extract_youtube_id(const std::string& url) {
    static const std::string kHttps = "https://";
    if (url.compare(0, kHttps.size(), kHttps) != 0) return std::nullopt;

    const std::size_t host_start = kHttps.size();
    const std::size_t host_end = url.find_first_of("/?#", host_start);
    if (host_end == std::string::npos) return std::nullopt;
    const std::string host = lower_ascii(url.substr(host_start, host_end - host_start));
    const std::string rest = url.substr(host_end); // starts with '/'

    auto id_from_path_after = [&](const std::string& prefix) -> std::optional<std::string> {
        if (rest.compare(0, prefix.size(), prefix) != 0) return std::nullopt;
        const std::string tail = rest.substr(prefix.size());
        const std::size_t stop = tail.find_first_of("/?#");
        const std::string id = (stop == std::string::npos) ? tail : tail.substr(0, stop);
        if (!valid_youtube_id(id)) return std::nullopt;
        return id;
    };

    auto id_from_query_v = [&]() -> std::optional<std::string> {
        const std::size_t q = rest.find('?');
        if (q == std::string::npos) return std::nullopt;
        const std::string query = rest.substr(q + 1);
        std::size_t pos = 0;
        while (pos <= query.size()) {
            const std::size_t amp = query.find('&', pos);
            const std::string kv =
                query.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
            if (kv.compare(0, 2, "v=") == 0) {
                std::string id = kv.substr(2);
                const std::size_t frag = id.find('#');
                if (frag != std::string::npos) id = id.substr(0, frag);
                if (!valid_youtube_id(id)) return std::nullopt;
                return id;
            }
            if (amp == std::string::npos) break;
            pos = amp + 1;
        }
        return std::nullopt;
    };

    // Exact host match only — never substring/suffix — so
    // "youtube.com.evil.example" or "evil.example/?host=youtube.com" cannot
    // pass as YouTube.
    if (host == "youtu.be") {
        return id_from_path_after("/");
    }
    if (host == "www.youtube.com" || host == "youtube.com" || host == "m.youtube.com") {
        if (rest.rfind("/watch", 0) == 0) return id_from_query_v();
        return id_from_path_after("/embed/");
    }
    if (host == "www.youtube-nocookie.com" || host == "youtube-nocookie.com") {
        return id_from_path_after("/embed/");
    }
    return std::nullopt;
}

bool FileStore::add_jukebox_track(const std::string& owner, const std::string& video_id,
                                   const std::string& queued_by) {
    if (!by_username_.count(owner)) return false;
    if (!valid_youtube_id(video_id)) return false;
    if (jukebox_[owner].size() >= kJukeboxQueueCap) return false;
    jukebox_[owner].push_back(JukeboxTrack{owner, video_id, now(), queued_by});
    flush();
    return true;
}

void FileStore::remove_jukebox_track(const std::string& owner, const std::string& video_id) {
    auto it = jukebox_.find(owner);
    if (it != jukebox_.end()) {
        auto& list = it->second;
        for (std::size_t i = 0; i < list.size(); ++i) {
            if (list[i].video_id == video_id) {
                list.erase(list.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
        }
    }
    flush();
}

std::vector<JukeboxTrack> FileStore::jukebox_of(const std::string& owner) const {
    auto it = jukebox_.find(owner);
    if (it == jukebox_.end()) return {};
    return it->second;
}

void FileStore::complete_jukebox_track(const std::string& owner, const std::string& video_id) {
    auto it = jukebox_.find(owner);
    if (it == jukebox_.end()) return;
    auto& list = it->second;
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (list[i].video_id != video_id) continue;
        const std::string queued_by = list[i].queued_by;
        auto& hist = jukebox_history_[owner];
        hist.push_back(JukeboxHistoryEntry{owner, video_id, queued_by, now()});
        if (hist.size() > kJukeboxHistoryCap) {
            hist.erase(hist.begin(), hist.begin() + static_cast<std::ptrdiff_t>(hist.size() - kJukeboxHistoryCap));
        }
        if (queued_by != owner) list.erase(list.begin() + static_cast<std::ptrdiff_t>(i));
        flush();
        return;
    }
}

void FileStore::skip_jukebox_track(const std::string& owner, const std::string& video_id) {
    auto it = jukebox_.find(owner);
    if (it == jukebox_.end() || video_id.empty()) return;
    auto& list = it->second;
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (list[i].video_id != video_id) continue;
        JukeboxTrack t = list[i];
        auto& hist = jukebox_history_[owner];
        hist.push_back(JukeboxHistoryEntry{owner, t.video_id, t.queued_by, now()});
        if (hist.size() > kJukeboxHistoryCap) {
            hist.erase(hist.begin(), hist.begin() + static_cast<std::ptrdiff_t>(hist.size() - kJukeboxHistoryCap));
        }
        list.erase(list.begin() + static_cast<std::ptrdiff_t>(i));
        if (t.queued_by == owner) list.push_back(t);
        flush();
        return;
    }
}

void FileStore::record_jukebox_play(const std::string& owner, const std::string& video_id,
                                    const std::string& queued_by) {
    if (owner.empty() || !valid_youtube_id(video_id)) return;
    auto& hist = jukebox_history_[owner];
    hist.push_back(JukeboxHistoryEntry{owner, video_id, queued_by, now()});
    if (hist.size() > kJukeboxHistoryCap) {
        hist.erase(hist.begin(), hist.begin() + static_cast<std::ptrdiff_t>(hist.size() - kJukeboxHistoryCap));
    }
    flush();
}

std::vector<JukeboxHistoryEntry> FileStore::jukebox_history_of(const std::string& owner) const {
    auto it = jukebox_history_.find(owner);
    if (it == jukebox_history_.end()) return {};
    return it->second;
}

bool FileStore::subscribe_jukebox(const std::string& subscriber, const std::string& owner) {
    if (subscriber.empty() || owner.empty() || subscriber == owner) return false;
    auto result = jukebox_subs_.insert({subscriber, owner});
    if (!result.second) return false;
    flush();
    return true;
}

bool FileStore::unsubscribe_jukebox(const std::string& subscriber, const std::string& owner) {
    if (jukebox_subs_.erase({subscriber, owner}) == 0) return false;
    flush();
    return true;
}

bool FileStore::is_subscribed_jukebox(const std::string& subscriber, const std::string& owner) const {
    return jukebox_subs_.count({subscriber, owner}) > 0;
}

std::vector<std::string> FileStore::jukebox_subscriptions_of(const std::string& subscriber) const {
    std::vector<std::string> out;
    for (const auto& p : jukebox_subs_) {
        if (p.first == subscriber) out.push_back(p.second);
    }
    return out;
}

std::size_t FileStore::jukebox_subscriber_count(const std::string& owner) const {
    std::size_t n = 0;
    for (const auto& p : jukebox_subs_) {
        if (p.second == owner) ++n;
    }
    return n;
}

bool FileStore::jukebox_autoplay(const std::string& username) const {
    auto it = jukebox_autoplay_.find(username);
    return it != jukebox_autoplay_.end() && it->second;
}

void FileStore::set_jukebox_autoplay(const std::string& username, bool on) {
    if (on) jukebox_autoplay_[username] = true;
    else jukebox_autoplay_.erase(username);
    flush();
}

std::string FileStore::jukebox_relay_target(const std::string& username) const {
    auto it = jukebox_relay_.find(username);
    return it == jukebox_relay_.end() ? "" : it->second;
}

void FileStore::set_jukebox_relay_target(const std::string& username, const std::string& target) {
    if (target.empty()) jukebox_relay_.erase(username);
    else jukebox_relay_[username] = target;
    flush();
}

std::string FileStore::jukebox_effective_owner(const std::string& username) const {
    auto target = jukebox_relay_target(username);
    if (target.empty() || target == username) return username;
    if (!by_username_.count(target)) return username;
    // One hop only: if the target relays right back (A->B->A), or has
    // blocked the account relaying to it, fall back rather than chase the
    // chain any further or serve a blocked party's queue.
    if (jukebox_relay_target(target) == username) return username;
    if (is_blocked(target, username)) return username;
    return target;
}

std::optional<JukeboxPos> FileStore::jukebox_pos(const std::string& station) const {
    auto it = jukebox_pos_.find(station);
    if (it == jukebox_pos_.end()) return std::nullopt;
    return it->second;
}

void FileStore::set_jukebox_pos(const std::string& station, const std::string& video_id,
                                 std::int64_t offset_sec, std::int64_t updated_at) {
    jukebox_pos_[station] = JukeboxPos{video_id, offset_sec, updated_at};
    flush();
}

void FileStore::clear_jukebox_pos(const std::string& station) {
    if (!jukebox_pos_.erase(station)) return;
    flush();
}

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_jukebox(const std::vector<std::string>& f, LoadPending& pending) {
    (void)pending;
    const std::string& tag = f[0];
    if (tag == "jt") {
        if (f.size() != 4 && f.size() != 5) throw std::runtime_error("LGP1: bad jt record");
        JukeboxTrack t;
        t.owner = pct_decode(f[1]);
        t.video_id = pct_decode(f[2]);
        t.added_at = parse_i64(f[3], "jukebox added_at");
        t.queued_by = f.size() == 5 ? pct_decode(f[4]) : t.owner;
        if (t.owner.empty() || !valid_youtube_id(t.video_id)) {
            throw std::runtime_error("LGP1: bad jt video_id");
        }
        jukebox_[t.owner].push_back(t);
        return true;
    }
    if (tag == "jh") {
        if (f.size() != 5) throw std::runtime_error("LGP1: bad jh record");
        JukeboxHistoryEntry h;
        h.owner = pct_decode(f[1]);
        h.video_id = pct_decode(f[2]);
        h.queued_by = pct_decode(f[3]);
        h.played_at = parse_i64(f[4], "jukebox history played_at");
        if (h.owner.empty() || !valid_youtube_id(h.video_id)) {
            throw std::runtime_error("LGP1: bad jh video_id");
        }
        jukebox_history_[h.owner].push_back(h);
        return true;
    }
    if (tag == "jsub") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad jsub record");
        jukebox_subs_.insert({pct_decode(f[1]), pct_decode(f[2])});
        return true;
    }
    if (tag == "jauto") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad jauto record");
        jukebox_autoplay_[pct_decode(f[1])] = parse_bit(f[2], "jauto");
        return true;
    }
    if (tag == "jrelay") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad jrelay record");
        auto target = pct_decode(f[2]);
        if (!target.empty()) jukebox_relay_[pct_decode(f[1])] = target;
        return true;
    }
    if (tag == "jpos") {
        if (f.size() != 5) throw std::runtime_error("LGP1: bad jpos record");
        JukeboxPos p;
        p.video_id = pct_decode(f[2]);
        p.offset_sec = parse_i64(f[3], "jpos offset_sec");
        p.updated_at = parse_i64(f[4], "jpos updated_at");
        if (!valid_youtube_id(p.video_id)) throw std::runtime_error("LGP1: bad jpos video_id");
        jukebox_pos_[pct_decode(f[1])] = p;
        return true;
    }
    return false;
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_jukebox(std::ostream& body) {
    for (const auto& [owner, list] : jukebox_) {
        if (!by_username_.count(owner)) continue;
        for (const auto& t : list) {
            body << "jt\t" << pct_encode(t.owner) << "\t" << pct_encode(t.video_id) << "\t"
                 << t.added_at << "\t" << pct_encode(t.queued_by) << "\n";
        }
    }
    for (const auto& [owner, list] : jukebox_history_) {
        if (!by_username_.count(owner) && owner.rfind("jbs:", 0) != 0) continue;
        for (const auto& h : list) {
            body << "jh\t" << pct_encode(h.owner) << "\t" << pct_encode(h.video_id) << "\t"
                 << pct_encode(h.queued_by) << "\t" << h.played_at << "\n";
        }
    }
    for (const auto& p : jukebox_subs_) {
        if (!by_username_.count(p.first) || !by_username_.count(p.second)) continue;
        body << "jsub\t" << pct_encode(p.first) << "\t" << pct_encode(p.second) << "\n";
    }
    for (const auto& [name, on] : jukebox_autoplay_) {
        if (!on || !by_username_.count(name)) continue;
        body << "jauto\t" << pct_encode(name) << "\t1\n";
    }
    for (const auto& [name, target] : jukebox_relay_) {
        if (target.empty() || !by_username_.count(name)) continue;
        body << "jrelay\t" << pct_encode(name) << "\t" << pct_encode(target) << "\n";
    }
    for (const auto& [station, p] : jukebox_pos_) {
        body << "jpos\t" << pct_encode(station) << "\t" << pct_encode(p.video_id) << "\t"
             << p.offset_sec << "\t" << p.updated_at << "\n";
    }
}

} // namespace liquidgov::platform
