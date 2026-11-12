// posts: Posts, reposts, attachments, audiences, replies, seconds.
// LGP1 record types: post reply repln sec rp pa pv pau px pf pg (format: the header comment of file_store.hpp).
// Split out of file_store.cpp 2026-11-12 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"
#include "liquidgov/platform/clock.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

std::size_t FileStore::post_count() const {
    return posts_.size();
}

std::uint64_t FileStore::add_post(const std::string& author, const std::string& text,
                                  const std::string& community,
                                  const std::string& repost_of_author, std::uint64_t repost_of_id,
                                  const std::string& attachment_blob_id, const std::string& attachment_mime,
                                  PostAudience audience, std::vector<std::string> exceptions,
                                  std::vector<std::string> friend_allowlist,
                                  std::vector<std::string> extra_groups) {
    Post p;
    p.id = next_post_id_++;
    p.author = author;
    p.ts = now();
    p.text = text;
    p.community = community;
    p.repost_of_author = repost_of_author;
    p.repost_of_id = repost_of_id;
    p.attachment_blob_id = attachment_blob_id;
    p.attachment_mime = attachment_mime;
    p.audience = audience;
    p.exceptions = std::move(exceptions);
    p.friend_allowlist = std::move(friend_allowlist);
    p.extra_groups = std::move(extra_groups);
    posts_.push_back(p);
    flush();
    return p.id;
}

bool FileStore::delete_post(std::uint64_t post_id, const std::string& actor) {
    auto it = std::find_if(posts_.begin(), posts_.end(),
                            [&](const Post& p) { return p.id == post_id; });
    if (it == posts_.end() || it->author != actor) return false;
    posts_.erase(it);
    replies_.erase(std::remove_if(replies_.begin(), replies_.end(),
                                   [&](const PostReply& r) { return r.post_id == post_id; }),
                   replies_.end());
    for (auto sit = seconds_.begin(); sit != seconds_.end();) {
        if (sit->first == post_id) sit = seconds_.erase(sit);
        else ++sit;
    }
    flush();
    return true;
}

// Whether `viewer` (a confirmed, non-blocked friend of the author -- caller
// already checked that) gets to see a Friends/Anybody post in a
// friends-graph context (this feed, or the profile's own Posts list).
// Anybody's px hide-list applies here too, same as on the profile ("must
// not see this post (feed or profile)") -- it only ever trims reach, never
// grants it. The account-wide posts_visibility OnlyMe blanket gates only
// the Friends case: an explicit per-post Anybody choice wins over a stale
// account-level toggle, same posture as the profile page (see its own
// per-post filter) rather than letting an old blanket silently override a
// fresh, explicit composer choice.
bool FileStore::friend_sees_post(const Post& p, const std::string& viewer) const {
    if (p.audience == PostAudience::Anybody)
        return std::find(p.exceptions.begin(), p.exceptions.end(), viewer) == p.exceptions.end();
    if (prefs_of(p.author).posts_visibility == Visibility::OnlyMe) return false;
    return p.friend_allowlist.empty() ||
        std::find(p.friend_allowlist.begin(), p.friend_allowlist.end(), viewer) != p.friend_allowlist.end();
}

std::vector<Post> FileStore::feed_for(const std::string& user, std::size_t limit) const {
    std::vector<Post> out;
    for (auto it = posts_.rbegin(); it != posts_.rend(); ++it) {
        if (it->audience == PostAudience::GroupsOnly) continue;
        const bool is_self = it->author == user;
        if (!is_self) {
            if (!are_friends(user, it->author)) continue;
            if (is_blocked(user, it->author)) continue;
            if (!friend_sees_post(*it, user)) continue;
        }
        out.push_back(*it);
        if (out.size() >= limit) break;
    }
    return out;
}

std::vector<Post> FileStore::feed_view(const std::string& user, std::size_t limit, FeedFilter filter) const {
    if (filter == FeedFilter::Friends) return feed_for(user, limit);
    std::set<std::string> my_slugs;
    for (const auto& c : communities_of(user)) my_slugs.insert(c.slug);
    std::vector<Post> out;
    for (auto it = posts_.rbegin(); it != posts_.rend(); ++it) {
        bool personal = false;
        if (it->audience != PostAudience::GroupsOnly) {
            const bool is_self = it->author == user;
            personal = is_self ||
                (are_friends(user, it->author) && !is_blocked(user, it->author) && friend_sees_post(*it, user));
        }
        bool committee_post = !it->community.empty() && my_slugs.count(it->community) > 0;
        if (!committee_post) {
            for (const auto& slug : it->extra_groups) {
                if (my_slugs.count(slug)) { committee_post = true; break; }
            }
        }
        bool include;
        if (filter == FeedFilter::Committees) include = committee_post;
        else include = personal || committee_post; // All
        if (!include) continue;
        out.push_back(*it);
        if (out.size() >= limit) break;
    }
    return out;
}

std::vector<Post> FileStore::posts_in(const std::string& community, std::size_t limit) const {
    std::vector<Post> out;
    for (auto it = posts_.rbegin(); it != posts_.rend(); ++it) {
        const bool matches = it->community == community ||
            std::find(it->extra_groups.begin(), it->extra_groups.end(), community) != it->extra_groups.end();
        if (matches) {
            out.push_back(*it);
            if (out.size() >= limit) break;
        }
    }
    return out;
}

std::vector<Post> FileStore::posts_by(const std::string& author) const {
    std::vector<Post> out;
    for (const auto& p : posts_) {
        if (p.author == author) out.push_back(p);
    }
    return out;
}

std::vector<Post> FileStore::recent_posts(std::size_t limit) const {
    std::vector<Post> out;
    for (auto it = posts_.rbegin(); it != posts_.rend() && out.size() < limit; ++it)
        out.push_back(*it);
    return out;
}

std::optional<Post> FileStore::post_by_id(std::uint64_t id) const {
    for (const auto& p : posts_) {
        if (p.id == id) return p;
    }
    return std::nullopt;
}

std::vector<Post> FileStore::all_posts_with_attachments() const {
    std::vector<Post> out;
    for (const auto& p : posts_) {
        if (!p.attachment_blob_id.empty()) out.push_back(p);
    }
    return out;
}

std::uint64_t FileStore::add_reply(std::uint64_t post_id, const std::string& author, const std::string& text) {
    PostReply r;
    r.id = next_reply_id_++;
    r.post_id = post_id;
    r.author = author;
    r.ts = now();
    r.text = text;
    replies_.push_back(r);
    flush();
    return r.id;
}

std::vector<PostReply> FileStore::replies_of(std::uint64_t post_id) const {
    std::vector<PostReply> out;
    for (const auto& r : replies_) {
        if (r.post_id == post_id) out.push_back(r);
    }
    return out;
}

void FileStore::toggle_second(std::uint64_t post_id, const std::string& user) {
    if (user.empty()) return;
    auto key = std::make_pair(post_id, user);
    if (seconds_.count(key)) seconds_.erase(key);
    else seconds_.insert(key);
    flush();
}

bool FileStore::has_seconded(std::uint64_t post_id, const std::string& user) const {
    return seconds_.count({post_id, user}) > 0;
}

int FileStore::second_count(std::uint64_t post_id) const {
    int n = 0;
    for (const auto& s : seconds_) {
        if (s.first == post_id) ++n;
    }
    return n;
}

std::vector<Post> FileStore::all_posts() const { return posts_; }

std::vector<PostReply> FileStore::recent_post_replies(std::size_t limit) const {
    std::vector<PostReply> out;
    for (auto it = replies_.rbegin(); it != replies_.rend() && out.size() < limit; ++it) out.push_back(*it);
    return out;
}

std::optional<PostReply> FileStore::post_reply_by_id(std::uint64_t id) const {
    for (const auto& r : replies_) if (r.id == id) return r;
    return std::nullopt;
}

bool FileStore::delete_post_reply(std::uint64_t reply_id) {
    auto it = std::find_if(replies_.begin(), replies_.end(), [&](const PostReply& r) { return r.id == reply_id; });
    if (it == replies_.end()) return false;
    replies_.erase(it);
    prune_pains(); // a problem suggestion read from this comment goes with it
    flush();
    return true;
}

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_posts(const std::vector<std::string>& f, LoadPending& pending) {
    auto& pending_attachments = pending.pending_attachments;
    auto& pending_audience = pending.pending_audience;
    auto& pending_exceptions = pending.pending_exceptions;
    auto& pending_extra_groups = pending.pending_extra_groups;
    auto& pending_friend_allowlist = pending.pending_friend_allowlist;
    auto& pending_reposts = pending.pending_reposts;
    const std::string& tag = f[0];
    if (tag == "post") {
        if (f.size() != 6) throw std::runtime_error("LGP1: bad post record");
        Post p;
        p.id = parse_u64(f[1], "post id");
        p.author = pct_decode(f[2]);
        p.ts = parse_i64(f[3], "post ts");
        p.text = pct_decode(f[4]);
        p.community = pct_decode(f[5]);
        posts_.push_back(p);
        if (p.id >= next_post_id_) next_post_id_ = p.id + 1;
        return true;
    }
    if (tag == "reply") {
        if (f.size() != 6) throw std::runtime_error("LGP1: bad reply record");
        PostReply r;
        r.id = parse_u64(f[1], "reply id");
        r.post_id = parse_u64(f[2], "reply post_id");
        r.author = pct_decode(f[3]);
        r.ts = parse_i64(f[4], "reply ts");
        r.text = pct_decode(f[5]);
        replies_.push_back(r);
        if (r.id >= next_reply_id_) next_reply_id_ = r.id + 1;
        return true;
    }
    if (tag == "repln") {
        if (f.size() != 2) throw std::runtime_error("LGP1: bad repln record");
        next_reply_id_ = parse_u64(f[1], "next_reply_id");
        if (next_reply_id_ == 0) next_reply_id_ = 1;
        return true;
    }
    if (tag == "sec") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad sec record");
        seconds_.insert({parse_u64(f[1], "sec post_id"), pct_decode(f[2])});
        return true;
    }
    if (tag == "rp") {
        // A separate tag (not a wider "post" record) specifically so
        // existing 6-field "post" records from before reposts existed
        // still load unchanged -- no format migration needed. Recorded
        // here and applied to posts_ in a fixup pass after the load
        // loop, since "rp" isn't guaranteed to appear after its "post".
        if (f.size() != 4) throw std::runtime_error("LGP1: bad rp record");
        pending_reposts[parse_u64(f[1], "rp post id")] =
            {pct_decode(f[2]), parse_u64(f[3], "rp origin id")};
        return true;
    }
    if (tag == "pa") {
        // Same reasoning as "rp": a separate tag so a "post" record
        // without an attachment never changes shape.
        if (f.size() != 4) throw std::runtime_error("LGP1: bad pa record");
        pending_attachments[parse_u64(f[1], "pa post id")] =
            {pct_decode(f[2]), pct_decode(f[3])};
        return true;
    }
    if (tag == "pv") {
        // Leftover from an earlier, since-superseded same-day draft of
        // per-post audience (a plain Visibility 0=Public/2=Friends,
        // before Groups exceptions/allowlists existed) -- no longer
        // written, but tolerated on load and converted into the
        // pau/PostAudience model below, same posture as "em". Only
        // pau's OWN value wins if a post somehow has both (never
        // written by the same version of this code, but the fixup pass
        // below prefers a real pau line over this fallback either way).
        if (f.size() != 3) throw std::runtime_error("LGP1: bad pv record");
        const auto vi = parse_u64(f[2], "pv visibility");
        const auto id = parse_u64(f[1], "pv post id");
        if (!pending_audience.count(id))
            pending_audience[id] = (vi == 0) ? PostAudience::Anybody : PostAudience::Friends;
        return true;
    }
    if (tag == "pau") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad pau record");
        const auto av = parse_u64(f[2], "pau audience");
        if (av > static_cast<std::uint64_t>(PostAudience::GroupsOnly))
            throw std::runtime_error("LGP1: bad pau audience value");
        pending_audience[parse_u64(f[1], "pau post id")] = static_cast<PostAudience>(av);
        return true;
    }
    if (tag == "px") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad px record");
        pending_exceptions[parse_u64(f[1], "px post id")].push_back(pct_decode(f[2]));
        return true;
    }
    if (tag == "pf") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad pf record");
        pending_friend_allowlist[parse_u64(f[1], "pf post id")].push_back(pct_decode(f[2]));
        return true;
    }
    if (tag == "pg") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad pg record");
        pending_extra_groups[parse_u64(f[1], "pg post id")].push_back(pct_decode(f[2]));
        return true;
    }
    return false;
}

// After every line is read (load() calls the finishers in a fixed order).
void FileStore::finish_posts(LoadPending& pending) {
    auto& pending_attachments = pending.pending_attachments;
    auto& pending_audience = pending.pending_audience;
    auto& pending_exceptions = pending.pending_exceptions;
    auto& pending_extra_groups = pending.pending_extra_groups;
    auto& pending_friend_allowlist = pending.pending_friend_allowlist;
    auto& pending_reposts = pending.pending_reposts;
    for (auto& p : posts_) {
        auto rit = pending_reposts.find(p.id);
        if (rit != pending_reposts.end()) {
            p.repost_of_author = rit->second.first;
            p.repost_of_id = rit->second.second;
        }
        auto ait = pending_attachments.find(p.id);
        if (ait != pending_attachments.end()) {
            p.attachment_blob_id = ait->second.first;
            p.attachment_mime = ait->second.second;
        }
        auto pauit = pending_audience.find(p.id);
        if (pauit != pending_audience.end()) p.audience = pauit->second;
        auto pxit = pending_exceptions.find(p.id);
        if (pxit != pending_exceptions.end()) p.exceptions = pxit->second;
        auto pfit = pending_friend_allowlist.find(p.id);
        if (pfit != pending_friend_allowlist.end()) p.friend_allowlist = pfit->second;
        auto pgit = pending_extra_groups.find(p.id);
        if (pgit != pending_extra_groups.end()) p.extra_groups = pgit->second;
    }
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_posts(std::ostream& body) {
    for (const auto& p : posts_) {
        body << "post\t" << p.id << "\t" << pct_encode(p.author) << "\t" << p.ts << "\t"
             << pct_encode(p.text) << "\t" << pct_encode(p.community) << "\n";
        if (!p.repost_of_author.empty()) {
            body << "rp\t" << p.id << "\t" << pct_encode(p.repost_of_author) << "\t"
                 << p.repost_of_id << "\n";
        }
        if (!p.attachment_blob_id.empty()) {
            body << "pa\t" << p.id << "\t" << pct_encode(p.attachment_blob_id) << "\t"
                 << pct_encode(p.attachment_mime) << "\n";
        }
        if (p.audience != PostAudience::Friends) {
            body << "pau\t" << p.id << "\t" << static_cast<int>(p.audience) << "\n";
        }
        for (const auto& ex : p.exceptions) {
            body << "px\t" << p.id << "\t" << pct_encode(ex) << "\n";
        }
        for (const auto& fr : p.friend_allowlist) {
            body << "pf\t" << p.id << "\t" << pct_encode(fr) << "\n";
        }
        for (const auto& slug : p.extra_groups) {
            body << "pg\t" << p.id << "\t" << pct_encode(slug) << "\n";
        }
    }
    body << "repln\t" << next_reply_id_ << "\n";
    for (const auto& r : replies_) {
        body << "reply\t" << r.id << "\t" << r.post_id << "\t" << pct_encode(r.author) << "\t"
             << r.ts << "\t" << pct_encode(r.text) << "\n";
    }
    for (const auto& s : seconds_) {
        body << "sec\t" << s.first << "\t" << pct_encode(s.second) << "\n";
    }
}

} // namespace liquidgov::platform
