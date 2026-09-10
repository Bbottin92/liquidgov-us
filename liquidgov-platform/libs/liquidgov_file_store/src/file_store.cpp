#include "liquidgov/platform/file_store.hpp"

#include "liquidgov/platform/clock.hpp"

#include <sodium.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

namespace liquidgov::platform {
namespace {

std::string pct_encode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string o;
    o.reserve(s.size());
    for (unsigned char c : s) {
        if (c == '%' || c == '\t' || c == '\n' || c == '\r') {
            o.push_back('%');
            o.push_back(hex[c >> 4]);
            o.push_back(hex[c & 0x0f]);
        } else {
            o.push_back(static_cast<char>(c));
        }
    }
    return o;
}

int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

std::string pct_decode(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%') {
            if (i + 2 >= s.size()) throw std::runtime_error("LGP1: truncated percent escape");
            const int hi = hex_val(s[i + 1]);
            const int lo = hex_val(s[i + 2]);
            if (hi < 0 || lo < 0) throw std::runtime_error("LGP1: bad percent escape");
            o.push_back(static_cast<char>((hi << 4) | lo));
            i += 2;
        } else {
            o.push_back(s[i]);
        }
    }
    return o;
}

std::string to_hex(const unsigned char* p, std::size_t n) {
    static const char* hex = "0123456789abcdef";
    std::string o;
    o.resize(n * 2);
    for (std::size_t i = 0; i < n; ++i) {
        o[i * 2] = hex[p[i] >> 4];
        o[i * 2 + 1] = hex[p[i] & 0x0f];
    }
    return o;
}

std::vector<std::string> split_tab(const std::string& line) {
    std::vector<std::string> f;
    std::string cur;
    for (char c : line) {
        if (c == '\t') {
            f.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    f.push_back(cur);
    return f;
}

std::uint64_t parse_u64(const std::string& s, const char* what) {
    if (s.empty()) throw std::runtime_error(std::string("LGP1: empty ") + what);
    std::size_t idx = 0;
    std::uint64_t v = 0;
    try {
        v = std::stoull(s, &idx, 10);
    } catch (...) {
        throw std::runtime_error(std::string("LGP1: bad ") + what);
    }
    if (idx != s.size()) throw std::runtime_error(std::string("LGP1: trailing junk in ") + what);
    return v;
}

std::int64_t parse_i64(const std::string& s, const char* what) {
    if (s.empty()) throw std::runtime_error(std::string("LGP1: empty ") + what);
    std::size_t idx = 0;
    std::int64_t v = 0;
    try {
        v = std::stoll(s, &idx, 10);
    } catch (...) {
        throw std::runtime_error(std::string("LGP1: bad ") + what);
    }
    if (idx != s.size()) throw std::runtime_error(std::string("LGP1: trailing junk in ") + what);
    return v;
}

bool parse_bit(const std::string& s, const char* what) {
    const auto v = parse_u64(s, what);
    if (v > 1) throw std::runtime_error(std::string("LGP1: bad ") + what);
    return v == 1;
}

void write_all(int fd, const std::string& data) {
    const char* p = data.data();
    std::size_t left = data.size();
    while (left > 0) {
        const ssize_t n = ::write(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error(std::string("store write failed: ") + std::strerror(errno));
        }
        if (n == 0) throw std::runtime_error("store write returned zero");
        p += n;
        left -= static_cast<std::size_t>(n);
    }
}

std::string lower_copy(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

bool starts_with_ci(const std::string& s, const std::string& prefix) {
    if (prefix.size() > s.size()) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        char a = s[i], b = prefix[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

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

bool valid_username(const std::string& username) {
    if (username.empty() || username.size() > 32) return false;
    for (unsigned char c : username) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

std::string fingerprint_phone(const std::string& e164) {
    if (sodium_init() < 0) throw std::runtime_error("libsodium init failed");
    unsigned char h[32];
    crypto_generichash(h, sizeof(h),
                       reinterpret_cast<const unsigned char*>(e164.data()), e164.size(),
                       nullptr, 0);
    return to_hex(h, sizeof(h));
}

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

FileStore::FileStore(std::string path) : path_(std::move(path)) { load(); }

bool FileStore::username_exists(const std::string& username) const {
    return by_username_.count(username) > 0;
}

std::optional<PlatformAccount> FileStore::find_by_username(const std::string& username) const {
    auto it = by_username_.find(username);
    if (it == by_username_.end()) return std::nullopt;
    return accounts_.at(it->second);
}

AccountId FileStore::create(const PlatformAccount& account) {
    AccountId id = next_id_++;
    PlatformAccount stored = account;
    stored.id = id;
    accounts_[id] = stored;
    by_username_[account.username] = id;
    try {
        flush();
    } catch (...) {
        accounts_.erase(id);
        by_username_.erase(account.username);
        if (next_id_ > 1) --next_id_;
        throw;
    }
    return id;
}

void FileStore::save_session(const Session& session) {
    sessions_[session.id] = session;
    flush();
}

std::vector<Session> FileStore::sessions_for(AccountId account) const {
    std::vector<Session> out;
    for (const auto& [id, s] : sessions_) {
        if (s.account == account) out.push_back(s);
    }
    return out;
}

void FileStore::revoke_session(const SessionId& id) {
    auto it = sessions_.find(id);
    if (it == sessions_.end()) return;
    it->second.revoked = true;
    flush();
}

std::optional<PlatformAccount> FileStore::find_by_id(AccountId id) const {
    auto it = accounts_.find(id);
    if (it == accounts_.end()) return std::nullopt;
    return it->second;
}

std::optional<Session> FileStore::get_session(const SessionId& id) const {
    auto it = sessions_.find(id);
    if (it == sessions_.end() || it->second.revoked) return std::nullopt;
    if (it->second.expires_at <= now()) return std::nullopt;
    return it->second;
}

void FileStore::touch_session(const SessionId& id) {
    auto it = sessions_.find(id);
    if (it == sessions_.end() || it->second.revoked) return;
    it->second.last_seen = now();
}

void FileStore::set_blurb(AccountId id, const std::string& blurb) {
    blurbs_[id] = blurb;
    flush();
}

std::string FileStore::blurb(AccountId id) const {
    auto it = blurbs_.find(id);
    return it == blurbs_.end() ? "" : it->second;
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

bool FileStore::set_password_hash(AccountId id, const std::string& password_hash) {
    auto it = accounts_.find(id);
    if (it == accounts_.end()) return false;
    if (password_hash.empty()) return false;
    it->second.password_hash = password_hash;
    flush();
    return true;
}

bool FileStore::are_friends(const std::string& a, const std::string& b) const {
    return friends_.count({a, b}) > 0 || friends_.count({b, a}) > 0;
}

std::vector<std::string> FileStore::friends_of(const std::string& user) const {
    std::vector<std::string> out;
    for (const auto& p : friends_) {
        if (p.first == user) out.push_back(p.second);
        else if (p.second == user) out.push_back(p.first);
    }
    return out;
}

void FileStore::add_friend_request(const std::string& from, const std::string& to) {
    if (from.empty() || to.empty() || from == to) return;
    if (are_friends(from, to)) return;
    if (is_blocked(from, to) || is_blocked(to, from)) return;
    for (const auto& r : requests_) {
        if (r.first == from && r.second == to) return;
    }
    requests_.push_back({from, to});
    auto p = prefs_of(to);
    if (p.notify_friend) add_notice(to, "friend_request", from, "");
    else flush();
}

bool FileStore::accept_friend_request(const std::string& from, const std::string& to) {
    bool found = false;
    std::vector<std::pair<std::string, std::string>> keep;
    keep.reserve(requests_.size());
    for (const auto& r : requests_) {
        if (r.first == from && r.second == to) {
            found = true;
        } else {
            keep.push_back(r);
        }
    }
    if (!found) return false;
    requests_.swap(keep);
    friends_.insert({to, from});
    auto p = prefs_of(from);
    if (p.notify_friend) add_notice(from, "friend_accept", to, "");
    else flush();
    return true;
}

void FileStore::drop_requests_between(const std::string& a, const std::string& b) {
    std::vector<std::pair<std::string, std::string>> keep;
    for (const auto& r : requests_) {
        if ((r.first == a && r.second == b) || (r.first == b && r.second == a)) continue;
        keep.push_back(r);
    }
    requests_.swap(keep);
}

void FileStore::strip_inner_both(const std::string& a, const std::string& b) {
    if (inner_.count(a)) inner_[a].erase(b);
    if (inner_.count(b)) inner_[b].erase(a);
}

void FileStore::unfriend(const std::string& actor, const std::string& other) {
    if (actor.empty() || other.empty() || actor == other) return;
    friends_.erase({actor, other});
    friends_.erase({other, actor});
    drop_requests_between(actor, other);
    strip_inner_both(actor, other);
    // Age-blind, uniform: only the actor leaves shared group conversations
    // (see messenger.hpp on_relationship_ended). block() calls this same
    // function, so a block gets identical conversation-leaving behavior —
    // no separate code path to keep in sync.
    auto actor_id = by_username_.find(actor);
    auto other_id = by_username_.find(other);
    if (actor_id != by_username_.end() && other_id != by_username_.end()) {
        on_relationship_ended(*this, actor_id->second, other_id->second);
    }
    flush();
}

std::vector<std::string> FileStore::incoming_requests(const std::string& user) const {
    std::vector<std::string> out;
    for (const auto& r : requests_) {
        if (r.second == user) out.push_back(r.first);
    }
    return out;
}

std::vector<std::string> FileStore::outgoing_requests(const std::string& user) const {
    std::vector<std::string> out;
    for (const auto& r : requests_) {
        if (r.first == user) out.push_back(r.second);
    }
    return out;
}

void FileStore::add_mail(const std::string& from, const std::string& to, const std::string& text,
                          const std::optional<std::string>& attachment_blob_id,
                          const std::optional<std::string>& attachment_mime) {
    MailMessage msg;
    msg.from = from;
    msg.to = to;
    msg.text = text;
    msg.ts = now();
    msg.attachment_blob_id = attachment_blob_id;
    msg.attachment_mime = attachment_mime;
    mail_.push_back(msg);
    auto p = prefs_of(to);
    if (p.notify_message) add_notice(to, "message", from, "");
    else flush();
}

std::vector<MailMessage> FileStore::mail_with(const std::string& a, const std::string& b) const {
    std::vector<MailMessage> out;
    for (const auto& m : mail_) {
        if ((m.from == a && m.to == b) || (m.from == b && m.to == a)) out.push_back(m);
    }
    return out;
}

std::vector<MailMessage> FileStore::mail_sent_by(const std::string& from) const {
    std::vector<MailMessage> out;
    for (const auto& m : mail_) {
        if (m.from == from) out.push_back(m);
    }
    return out;
}

std::optional<MailMessage> FileStore::find_mail_attachment(const std::string& user,
                                                            const std::string& blob_id) const {
    for (const auto& m : mail_) {
        if (!m.attachment_blob_id || *m.attachment_blob_id != blob_id) continue;
        if (m.from == user || m.to == user) return m;
    }
    return std::nullopt;
}

std::vector<MailMessage> FileStore::all_mail_with_attachments() const {
    std::vector<MailMessage> out;
    for (const auto& m : mail_) {
        if (m.attachment_blob_id) out.push_back(m);
    }
    return out;
}

void FileStore::block(const std::string& actor, const std::string& other) {
    if (actor.empty() || other.empty() || actor == other) return;
    unfriend(actor, other);
    blocks_.insert({actor, other});
    flush();
}

bool FileStore::is_blocked(const std::string& blocker, const std::string& blocked) const {
    return blocks_.count({blocker, blocked}) > 0;
}

std::vector<std::string> FileStore::blocked_by(const std::string& actor) const {
    std::vector<std::string> out;
    for (const auto& p : blocks_) {
        if (p.first == actor) out.push_back(p.second);
    }
    return out;
}

void FileStore::unblock(const std::string& actor, const std::string& other) {
    blocks_.erase({actor, other});
    flush();
}

bool FileStore::add_inner(const std::string& owner, const std::string& friend_name) {
    if (!are_friends(owner, friend_name)) return false;
    auto& set = inner_[owner];
    if (set.count(friend_name)) return true;
    if (static_cast<int>(set.size()) >= kInnerCircleCap) return false;
    set.insert(friend_name);
    flush();
    return true;
}

void FileStore::remove_inner(const std::string& owner, const std::string& friend_name) {
    if (inner_.count(owner)) inner_[owner].erase(friend_name);
    flush();
}

std::vector<std::string> FileStore::inner_of(const std::string& owner) const {
    std::vector<std::string> out;
    auto it = inner_.find(owner);
    if (it == inner_.end()) return out;
    for (const auto& n : it->second) out.push_back(n);
    return out;
}

AccountPrefs FileStore::prefs_of(const std::string& username) const {
    auto it = prefs_.find(username);
    if (it == prefs_.end()) return AccountPrefs{};
    return it->second;
}

void FileStore::set_prefs(const std::string& username, const AccountPrefs& p) {
    prefs_[username] = p;
    flush();
}

bool FileStore::is_findable_exact(const std::string& requester, const std::string& target) const {
    if (requester.empty() || target.empty()) return false;
    if (!username_exists(target)) return false;
    if (requester == target) return true;
    if (is_blocked(requester, target)) return false;
    if (are_friends(requester, target)) return true;
    auto p = prefs_of(target);
    return p.findability != Findability::FriendsOnly;
}

std::vector<std::string> FileStore::typeahead(const std::string& requester, const std::string& prefix) const {
    std::vector<std::string> out;
    if (prefix.size() < 3) return out;
    for (const auto& [name, id] : by_username_) {
        if (!starts_with_ci(name, prefix)) continue;
        if (is_blocked(requester, name)) continue;
        if (requester == name) {
            out.push_back(name);
            continue;
        }
        if (are_friends(requester, name)) {
            out.push_back(name);
            continue;
        }
        auto p = prefs_of(name);
        if (p.findability == Findability::Typeahead) out.push_back(name);
    }
    std::sort(out.begin(), out.end(), [&](const std::string& a, const std::string& b) {
        const bool ea = lower_copy(a) == lower_copy(prefix);
        const bool eb = lower_copy(b) == lower_copy(prefix);
        if (ea != eb) return ea;
        const bool fa = are_friends(requester, a);
        const bool fb = are_friends(requester, b);
        if (fa != fb) return fa;
        return a < b;
    });
    if (out.size() > 20) out.resize(20);
    return out;
}

std::uint64_t FileStore::add_post(const std::string& author, const std::string& text,
                                  const std::string& community,
                                  const std::string& repost_of_author, std::uint64_t repost_of_id,
                                  const std::string& attachment_blob_id, const std::string& attachment_mime) {
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
    posts_.push_back(p);
    flush();
    return p.id;
}

std::vector<Post> FileStore::feed_for(const std::string& user, std::size_t limit) const {
    std::vector<Post> out;
    for (auto it = posts_.rbegin(); it != posts_.rend(); ++it) {
        if (!it->community.empty()) continue;
        if (it->author == user || are_friends(user, it->author)) {
            if (is_blocked(user, it->author)) continue;
            out.push_back(*it);
            if (out.size() >= limit) break;
        }
    }
    return out;
}

std::vector<Post> FileStore::posts_in(const std::string& community, std::size_t limit) const {
    std::vector<Post> out;
    for (auto it = posts_.rbegin(); it != posts_.rend(); ++it) {
        if (it->community == community) {
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

std::optional<Post> FileStore::post_by_id(std::uint64_t id) const {
    for (const auto& p : posts_) {
        if (p.id == id) return p;
    }
    return std::nullopt;
}

bool FileStore::create_community(const Community& c) {
    if (!valid_username(c.slug)) return false;
    if (communities_.count(c.slug)) return false;
    communities_[c.slug] = c;
    members_[c.slug].insert(c.owner);
    flush();
    return true;
}

std::optional<Community> FileStore::community(const std::string& slug) const {
    auto it = communities_.find(slug);
    if (it == communities_.end()) return std::nullopt;
    return it->second;
}

std::vector<Community> FileStore::all_communities() const {
    std::vector<Community> out;
    for (const auto& [slug, c] : communities_) out.push_back(c);
    return out;
}

bool FileStore::is_member(const std::string& slug, const std::string& username) const {
    auto it = members_.find(slug);
    if (it == members_.end()) return false;
    return it->second.count(username) > 0;
}

bool FileStore::join_community(const std::string& slug, const std::string& username) {
    if (!communities_.count(slug)) return false;
    members_[slug].insert(username);
    flush();
    return true;
}

bool FileStore::invite_community(const std::string& slug, const std::string& actor,
                                 const std::string& target) {
    if (!communities_.count(slug)) return false;
    if (!is_member(slug, actor)) return false;
    if (!username_exists(target)) return false;
    if (is_blocked(actor, target) || is_blocked(target, actor)) return false;
    auto p = prefs_of(target);
    if (p.group_invite_confirm) return false;
    members_[slug].insert(target);
    if (p.notify_group) add_notice(target, "group_invite", actor, slug);
    else flush();
    return true;
}

void FileStore::leave_community(const std::string& slug, const std::string& username) {
    auto it = communities_.find(slug);
    if (it == communities_.end()) return;
    if (it->second.owner == username) return; // owner stays until delete
    if (members_.count(slug)) members_[slug].erase(username);
    flush();
}

bool FileStore::delete_community(const std::string& slug, const std::string& actor) {
    auto it = communities_.find(slug);
    if (it == communities_.end()) return false;
    if (it->second.owner != actor) return false;
    communities_.erase(it);
    members_.erase(slug);
    std::vector<Post> keep;
    for (const auto& p : posts_) {
        if (p.community != slug) keep.push_back(p);
    }
    posts_.swap(keep);
    flush();
    return true;
}

std::vector<std::string> FileStore::members_of(const std::string& slug) const {
    std::vector<std::string> out;
    auto it = members_.find(slug);
    if (it == members_.end()) return out;
    for (const auto& n : it->second) out.push_back(n);
    return out;
}

std::vector<Community> FileStore::communities_of(const std::string& username) const {
    std::vector<Community> out;
    for (const auto& [slug, set] : members_) {
        if (set.count(username) && communities_.count(slug)) out.push_back(communities_.at(slug));
    }
    return out;
}

void FileStore::add_notice(const std::string& to, const std::string& type, const std::string& from,
                           const std::string& extra) {
    Notice n;
    n.id = next_note_id_++;
    n.to = to;
    n.type = type;
    n.from = from;
    n.ts = now();
    n.read = false;
    n.extra = extra;
    notices_.push_back(n);
    flush();
}

std::vector<Notice> FileStore::notices_for(const std::string& user, std::size_t limit) const {
    std::vector<Notice> out;
    for (auto it = notices_.rbegin(); it != notices_.rend(); ++it) {
        if (it->to == user) {
            out.push_back(*it);
            if (out.size() >= limit) break;
        }
    }
    return out;
}

int FileStore::unread_count(const std::string& user) const {
    int n = 0;
    for (const auto& note : notices_) {
        if (note.to == user && !note.read) ++n;
    }
    return n;
}

void FileStore::mark_notices_read(const std::string& user) {
    bool any = false;
    for (auto& note : notices_) {
        if (note.to == user && !note.read) {
            note.read = true;
            any = true;
        }
    }
    if (any) flush();
}

void FileStore::set_phone_fp(AccountId id, const std::string& fp) {
    phone_fp_[id] = fp;
    flush();
}

bool FileStore::phone_quarantined(const std::string& fp) const {
    if (fp.empty()) return false;
    auto it = quarantine_.find(fp);
    if (it == quarantine_.end()) return false;
    return it->second > now();
}

bool FileStore::delete_account(AccountId id) {
    auto it = accounts_.find(id);
    if (it == accounts_.end()) return false;
    const std::string name = it->second.username;
    auto fp_it = phone_fp_.find(id);
    if (fp_it != phone_fp_.end() && !fp_it->second.empty()) {
        quarantine_[fp_it->second] = now() + kPhoneQuarantineSeconds;
    }
    for (auto& m : mail_) {
        if (m.from == name) m.from = "[deleted account]";
        if (m.to == name) m.to = "[deleted account]";
    }
    std::vector<std::pair<std::string, std::string>> keep_req;
    for (const auto& r : requests_) {
        if (r.first == name || r.second == name) continue;
        keep_req.push_back(r);
    }
    requests_.swap(keep_req);
    std::set<std::pair<std::string, std::string>> keep_fr;
    for (const auto& p : friends_) {
        if (p.first == name || p.second == name) continue;
        keep_fr.insert(p);
    }
    friends_.swap(keep_fr);
    inner_.erase(name);
    for (auto& [owner, set] : inner_) set.erase(name);
    std::set<std::pair<std::string, std::string>> keep_blk;
    for (const auto& p : blocks_) {
        if (p.first == name || p.second == name) continue;
        keep_blk.insert(p);
    }
    blocks_.swap(keep_blk);
    prefs_.erase(name);
    for (auto& [slug, set] : members_) set.erase(name);
    std::vector<Post> keep_posts;
    for (auto& p : posts_) {
        if (p.author == name) p.author = "[deleted account]";
        if (p.repost_of_author == name) p.repost_of_author = "[deleted account]";
        keep_posts.push_back(p);
    }
    posts_.swap(keep_posts);
    std::vector<SessionId> drop;
    for (const auto& [sid, s] : sessions_) {
        if (s.account == id) drop.push_back(sid);
    }
    for (const auto& sid : drop) sessions_.erase(sid);
    phone_fp_.erase(id);
    blurbs_.erase(id);
    author_css_.erase(id);
    canvas_frames_.erase(id);
    canvas_timelapse_.erase(id);
    avatars_.erase(id);
    jukebox_.erase(name);
    by_username_.erase(name);
    accounts_.erase(it);
    flush();
    return true;
}

void FileStore::load() {
    next_id_ = 1;
    next_post_id_ = 1;
    next_note_id_ = 1;
    next_conv_id_ = 1;
    next_message_id_ = 1;
    by_username_.clear();
    accounts_.clear();
    sessions_.clear();
    blurbs_.clear();
    author_css_.clear();
    canvas_frames_.clear();
    canvas_timelapse_.clear();
    phone_fp_.clear();
    quarantine_.clear();
    friends_.clear();
    requests_.clear();
    mail_.clear();
    blocks_.clear();
    inner_.clear();
    prefs_.clear();
    posts_.clear();
    communities_.clear();
    members_.clear();
    notices_.clear();
    avatars_.clear();
    jukebox_.clear();
    conversations_.clear();
    messages_.clear();

    std::ifstream in(path_, std::ios::binary);
    if (!in) return;

    std::string line;
    if (!std::getline(in, line)) return;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "LGP1") {
        throw std::runtime_error("LGP1: bad magic in " + path_ + " (refusing to wipe)");
    }

    std::map<std::uint64_t, std::pair<std::string, std::uint64_t>> pending_reposts;
    std::map<std::uint64_t, std::pair<std::string, std::string>> pending_attachments;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto f = split_tab(line);
        if (f.empty()) throw std::runtime_error("LGP1: empty record");
        const std::string& tag = f[0];
        if (tag == "n") {
            if (f.size() != 2) throw std::runtime_error("LGP1: bad n record");
            next_id_ = parse_u64(f[1], "next_id");
            if (next_id_ == 0) next_id_ = 1;
        } else if (tag == "a") {
            if (f.size() != 7) throw std::runtime_error("LGP1: bad a record");
            PlatformAccount acc;
            acc.id = parse_u64(f[1], "account id");
            acc.username = pct_decode(f[2]);
            acc.password_hash = pct_decode(f[3]);
            acc.phone_verified = parse_bit(f[4], "phone_verified");
            acc.created_at = parse_i64(f[5], "created_at");
            if (acc.id == 0 || acc.username.empty() || acc.password_hash.empty()) {
                throw std::runtime_error("LGP1: incomplete account");
            }
            if (accounts_.count(acc.id) || by_username_.count(acc.username)) {
                throw std::runtime_error("LGP1: duplicate account");
            }
            accounts_[acc.id] = acc;
            by_username_[acc.username] = acc.id;
            blurbs_[acc.id] = pct_decode(f[6]);
            if (acc.id >= next_id_) next_id_ = acc.id + 1;
        } else if (tag == "s") {
            if (f.size() != 8) throw std::runtime_error("LGP1: bad s record");
            Session s;
            s.id = pct_decode(f[1]);
            s.account = parse_u64(f[2], "session account");
            s.created_at = parse_i64(f[3], "session created_at");
            s.last_seen = parse_i64(f[4], "session last_seen");
            s.expires_at = parse_i64(f[5], "session expires_at");
            s.revoked = parse_bit(f[6], "revoked");
            s.device_label = pct_decode(f[7]);
            if (s.id.empty()) throw std::runtime_error("LGP1: empty session id");
            sessions_[s.id] = s;
        } else if (tag == "f") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad f record");
            friends_.insert({pct_decode(f[1]), pct_decode(f[2])});
        } else if (tag == "r") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad r record");
            requests_.push_back({pct_decode(f[1]), pct_decode(f[2])});
        } else if (tag == "m") {
            if (f.size() != 4 && f.size() != 5) throw std::runtime_error("LGP1: bad m record");
            MailMessage msg;
            msg.from = pct_decode(f[1]);
            msg.to = pct_decode(f[2]);
            msg.text = pct_decode(f[3]);
            msg.ts = (f.size() == 5) ? parse_i64(f[4], "mail ts") : 0;
            mail_.push_back(msg);
        } else if (tag == "ma") {
            if (f.size() != 7) throw std::runtime_error("LGP1: bad ma record");
            MailMessage msg;
            msg.from = pct_decode(f[1]);
            msg.to = pct_decode(f[2]);
            msg.text = pct_decode(f[3]);
            msg.ts = parse_i64(f[4], "mail attachment ts");
            std::string blob_id = pct_decode(f[5]);
            if (blob_id.empty()) throw std::runtime_error("LGP1: empty ma blob_id");
            msg.attachment_blob_id = blob_id;
            msg.attachment_mime = pct_decode(f[6]);
            mail_.push_back(msg);
        } else if (tag == "ph") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad ph record");
            phone_fp_[parse_u64(f[1], "ph id")] = pct_decode(f[2]);
        } else if (tag == "pq") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad pq record");
            quarantine_[pct_decode(f[1])] = parse_i64(f[2], "pq until");
        } else if (tag == "post") {
            if (f.size() != 6) throw std::runtime_error("LGP1: bad post record");
            Post p;
            p.id = parse_u64(f[1], "post id");
            p.author = pct_decode(f[2]);
            p.ts = parse_i64(f[3], "post ts");
            p.text = pct_decode(f[4]);
            p.community = pct_decode(f[5]);
            posts_.push_back(p);
            if (p.id >= next_post_id_) next_post_id_ = p.id + 1;
        } else if (tag == "pn") {
            if (f.size() != 2) throw std::runtime_error("LGP1: bad pn record");
            next_post_id_ = parse_u64(f[1], "next_post_id");
            if (next_post_id_ == 0) next_post_id_ = 1;
        } else if (tag == "rp") {
            // A separate tag (not a wider "post" record) specifically so
            // existing 6-field "post" records from before reposts existed
            // still load unchanged -- no format migration needed. Recorded
            // here and applied to posts_ in a fixup pass after the load
            // loop, since "rp" isn't guaranteed to appear after its "post".
            if (f.size() != 4) throw std::runtime_error("LGP1: bad rp record");
            pending_reposts[parse_u64(f[1], "rp post id")] =
                {pct_decode(f[2]), parse_u64(f[3], "rp origin id")};
        } else if (tag == "pa") {
            // Same reasoning as "rp": a separate tag so a "post" record
            // without an attachment never changes shape.
            if (f.size() != 4) throw std::runtime_error("LGP1: bad pa record");
            pending_attachments[parse_u64(f[1], "pa post id")] =
                {pct_decode(f[2]), pct_decode(f[3])};
        } else if (tag == "comm") {
            if (f.size() != 6) throw std::runtime_error("LGP1: bad comm record");
            Community c;
            c.slug = pct_decode(f[1]);
            c.name = pct_decode(f[2]);
            c.owner = pct_decode(f[3]);
            c.about = pct_decode(f[4]);
            c.created_at = parse_i64(f[5], "comm created");
            communities_[c.slug] = c;
        } else if (tag == "cm") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad cm record");
            members_[pct_decode(f[1])].insert(pct_decode(f[2]));
        } else if (tag == "blk") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad blk record");
            blocks_.insert({pct_decode(f[1]), pct_decode(f[2])});
        } else if (tag == "inner") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad inner record");
            inner_[pct_decode(f[1])].insert(pct_decode(f[2]));
        } else if (tag == "pref") {
            if (f.size() != 9) throw std::runtime_error("LGP1: bad pref record");
            AccountPrefs p;
            const auto fv = parse_u64(f[2], "findability");
            if (fv > 2) throw std::runtime_error("LGP1: bad findability");
            p.findability = static_cast<Findability>(fv);
            p.friends_only_dm = parse_bit(f[3], "friends_only_dm");
            p.group_invite_confirm = parse_bit(f[4], "group_invite_confirm");
            p.presence_hide = parse_bit(f[5], "presence_hide");
            p.notify_friend = parse_bit(f[6], "n_friend");
            p.notify_message = parse_bit(f[7], "n_message");
            p.notify_group = parse_bit(f[8], "n_group");
            prefs_[pct_decode(f[1])] = p;
        } else if (tag == "note") {
            if (f.size() != 8) throw std::runtime_error("LGP1: bad note record");
            Notice n;
            n.id = parse_u64(f[1], "note id");
            n.to = pct_decode(f[2]);
            n.type = pct_decode(f[3]);
            n.from = pct_decode(f[4]);
            n.ts = parse_i64(f[5], "note ts");
            n.read = parse_bit(f[6], "note read");
            n.extra = pct_decode(f[7]);
            notices_.push_back(n);
            if (n.id >= next_note_id_) next_note_id_ = n.id + 1;
        } else if (tag == "nn") {
            if (f.size() != 2) throw std::runtime_error("LGP1: bad nn record");
            next_note_id_ = parse_u64(f[1], "next_note_id");
            if (next_note_id_ == 0) next_note_id_ = 1;
        } else if (tag == "ac") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad ac record");
            AccountId id = parse_u64(f[1], "author_css account");
            if (id == 0) throw std::runtime_error("LGP1: empty ac record");
            author_css_[id] = pct_decode(f[2]);
        } else if (tag == "cvf") {
            if (f.size() != 4) throw std::runtime_error("LGP1: bad cvf record");
            AccountId id = parse_u64(f[1], "canvas frame account");
            int idx = static_cast<int>(parse_u64(f[2], "canvas frame index"));
            std::string blob_id = pct_decode(f[3]);
            if (id == 0 || blob_id.empty()) throw std::runtime_error("LGP1: empty cvf record");
            auto& frames = canvas_frames_[id];
            if (static_cast<int>(frames.size()) <= idx) frames.resize(idx + 1);
            frames[idx] = blob_id;
        } else if (tag == "cvm") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad cvm record");
            AccountId id = parse_u64(f[1], "canvas mode account");
            if (id == 0) throw std::runtime_error("LGP1: empty cvm record");
            canvas_timelapse_[id] = parse_bit(f[2], "canvas timelapse");
        } else if (tag == "av") {
            if (f.size() != 4) throw std::runtime_error("LGP1: bad av record");
            Avatar av;
            av.owner = parse_u64(f[1], "avatar owner");
            av.blob_id = pct_decode(f[2]);
            av.created_at = parse_i64(f[3], "avatar ts");
            if (av.owner == 0 || av.blob_id.empty()) throw std::runtime_error("LGP1: empty av record");
            avatars_[av.owner] = av;
        } else if (tag == "jt") {
            if (f.size() != 4) throw std::runtime_error("LGP1: bad jt record");
            JukeboxTrack t;
            t.owner = pct_decode(f[1]);
            t.video_id = pct_decode(f[2]);
            t.added_at = parse_i64(f[3], "jukebox added_at");
            if (t.owner.empty() || !valid_youtube_id(t.video_id)) {
                throw std::runtime_error("LGP1: bad jt video_id");
            }
            jukebox_[t.owner].push_back(t);
        } else if (tag == "gcn") {
            if (f.size() != 2) throw std::runtime_error("LGP1: bad gcn record");
            next_conv_id_ = parse_u64(f[1], "next_conversation_id");
            if (next_conv_id_ == 0) next_conv_id_ = 1;
        } else if (tag == "gmsn") {
            if (f.size() != 2) throw std::runtime_error("LGP1: bad gmsn record");
            next_message_id_ = parse_u64(f[1], "next_message_id");
            if (next_message_id_ == 0) next_message_id_ = 1;
        } else if (tag == "gc") {
            if (f.size() != 4) throw std::runtime_error("LGP1: bad gc record");
            Conversation conv;
            conv.id = parse_u64(f[1], "conversation id");
            conv.is_group = parse_bit(f[2], "conversation is_group");
            conv.created_at = parse_i64(f[3], "conversation created_at");
            if (conv.id == 0) throw std::runtime_error("LGP1: empty gc id");
            if (conversations_.count(conv.id)) throw std::runtime_error("LGP1: duplicate conversation");
            conversations_[conv.id] = conv;
            if (conv.id >= next_conv_id_) next_conv_id_ = conv.id + 1;
        } else if (tag == "gcm") {
            if (f.size() != 4) throw std::runtime_error("LGP1: bad gcm record");
            const auto conv_id = parse_u64(f[1], "gcm conversation id");
            auto it = conversations_.find(conv_id);
            if (it == conversations_.end()) throw std::runtime_error("LGP1: gcm with no conversation");
            ConversationMember m;
            m.account = parse_u64(f[2], "gcm account");
            const auto state = parse_u64(f[3], "gcm state");
            if (state > 1) throw std::runtime_error("LGP1: bad gcm state");
            m.state = static_cast<MemberState>(state);
            it->second.members.push_back(m);
        } else if (tag == "gmsg") {
            if (f.size() != 10) throw std::runtime_error("LGP1: bad gmsg record");
            Message msg;
            msg.id = parse_u64(f[1], "message id");
            msg.conversation_id = parse_u64(f[2], "message conversation id");
            msg.sender = parse_u64(f[3], "message sender");
            msg.sent_at = parse_i64(f[4], "message sent_at");
            msg.edited_at = parse_i64(f[5], "message edited_at");
            const auto deletion = parse_u64(f[6], "message deletion");
            if (deletion > 2) throw std::runtime_error("LGP1: bad message deletion state");
            msg.deletion = static_cast<DeletionState>(deletion);
            msg.deleted_at = parse_i64(f[7], "message deleted_at");
            msg.under_review = parse_bit(f[8], "message under_review");
            msg.text = pct_decode(f[9]);
            if (msg.id == 0) throw std::runtime_error("LGP1: empty gmsg id");
            if (messages_.count(msg.id)) throw std::runtime_error("LGP1: duplicate message");
            messages_[msg.id] = msg;
            if (msg.id >= next_message_id_) next_message_id_ = msg.id + 1;
        } else if (tag == "gmh") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad gmh record");
            const auto msg_id = parse_u64(f[1], "gmh message id");
            auto it = messages_.find(msg_id);
            if (it == messages_.end()) throw std::runtime_error("LGP1: gmh with no message");
            it->second.edit_history.push_back(pct_decode(f[2]));
        } else {
            throw std::runtime_error("LGP1: unknown record type '" + tag + "'");
        }
    }
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
    }
}

void FileStore::flush() {
    namespace fs = std::filesystem;
    fs::path p(path_);
    if (p.has_parent_path() && !p.parent_path().empty()) {
        fs::create_directories(p.parent_path());
    }
    const std::string tmp = path_ + ".tmp";

    std::ostringstream body;
    body << "LGP1\n";
    body << "n\t" << next_id_ << "\n";
    body << "pn\t" << next_post_id_ << "\n";
    body << "nn\t" << next_note_id_ << "\n";
    body << "gcn\t" << next_conv_id_ << "\n";
    body << "gmsn\t" << next_message_id_ << "\n";
    for (const auto& [id, acc] : accounts_) {
        const std::string b = blurbs_.count(id) ? blurbs_.at(id) : "";
        body << "a\t" << acc.id << "\t" << pct_encode(acc.username) << "\t"
             << pct_encode(acc.password_hash) << "\t" << (acc.phone_verified ? 1 : 0) << "\t"
             << acc.created_at << "\t" << pct_encode(b) << "\n";
    }
    for (const auto& [id, fp] : phone_fp_) {
        body << "ph\t" << id << "\t" << pct_encode(fp) << "\n";
    }
    for (const auto& [fp, until] : quarantine_) {
        if (until <= now()) continue;
        body << "pq\t" << pct_encode(fp) << "\t" << until << "\n";
    }
    for (const auto& [id, s] : sessions_) {
        if (s.revoked) continue;
        if (s.expires_at <= now()) continue;
        if (!accounts_.count(s.account)) continue;
        body << "s\t" << pct_encode(s.id) << "\t" << s.account << "\t" << s.created_at << "\t"
             << s.last_seen << "\t" << s.expires_at << "\t" << (s.revoked ? 1 : 0) << "\t"
             << pct_encode(s.device_label) << "\n";
    }
    for (const auto& fr : friends_) {
        body << "f\t" << pct_encode(fr.first) << "\t" << pct_encode(fr.second) << "\n";
    }
    for (const auto& r : requests_) {
        body << "r\t" << pct_encode(r.first) << "\t" << pct_encode(r.second) << "\n";
    }
    for (const auto& m : mail_) {
        if (m.attachment_blob_id) {
            body << "ma\t" << pct_encode(m.from) << "\t" << pct_encode(m.to) << "\t"
                 << pct_encode(m.text) << "\t" << m.ts << "\t" << pct_encode(*m.attachment_blob_id)
                 << "\t" << pct_encode(m.attachment_mime.value_or("")) << "\n";
        } else {
            body << "m\t" << pct_encode(m.from) << "\t" << pct_encode(m.to) << "\t"
                 << pct_encode(m.text) << "\t" << m.ts << "\n";
        }
    }
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
    }
    for (const auto& [slug, c] : communities_) {
        body << "comm\t" << pct_encode(c.slug) << "\t" << pct_encode(c.name) << "\t"
             << pct_encode(c.owner) << "\t" << pct_encode(c.about) << "\t" << c.created_at << "\n";
    }
    for (const auto& [slug, set] : members_) {
        for (const auto& u : set) {
            body << "cm\t" << pct_encode(slug) << "\t" << pct_encode(u) << "\n";
        }
    }
    for (const auto& b : blocks_) {
        body << "blk\t" << pct_encode(b.first) << "\t" << pct_encode(b.second) << "\n";
    }
    for (const auto& [owner, set] : inner_) {
        for (const auto& u : set) {
            body << "inner\t" << pct_encode(owner) << "\t" << pct_encode(u) << "\n";
        }
    }
    for (const auto& [name, p] : prefs_) {
        body << "pref\t" << pct_encode(name) << "\t" << static_cast<int>(p.findability) << "\t"
             << (p.friends_only_dm ? 1 : 0) << "\t" << (p.group_invite_confirm ? 1 : 0) << "\t"
             << (p.presence_hide ? 1 : 0) << "\t" << (p.notify_friend ? 1 : 0) << "\t"
             << (p.notify_message ? 1 : 0) << "\t" << (p.notify_group ? 1 : 0) << "\n";
    }
    for (const auto& n : notices_) {
        body << "note\t" << n.id << "\t" << pct_encode(n.to) << "\t" << pct_encode(n.type) << "\t"
             << pct_encode(n.from) << "\t" << n.ts << "\t" << (n.read ? 1 : 0) << "\t"
             << pct_encode(n.extra) << "\n";
    }
    for (const auto& [id, av] : avatars_) {
        if (!accounts_.count(id)) continue;
        body << "av\t" << av.owner << "\t" << pct_encode(av.blob_id) << "\t" << av.created_at << "\n";
    }
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
    for (const auto& [owner, list] : jukebox_) {
        if (!by_username_.count(owner)) continue;
        for (const auto& t : list) {
            body << "jt\t" << pct_encode(t.owner) << "\t" << pct_encode(t.video_id) << "\t"
                 << t.added_at << "\n";
        }
    }
    // Conversations/messages are never pruned when an account is deleted —
    // same "keep the row, relabel at render time" choice as mail_/posts_
    // above. The numeric sender/member id just stops resolving through
    // find_by_id; the site layer renders that as "[deleted account]".
    for (const auto& [id, conv] : conversations_) {
        body << "gc\t" << conv.id << "\t" << (conv.is_group ? 1 : 0) << "\t" << conv.created_at << "\n";
    }
    for (const auto& [id, conv] : conversations_) {
        for (const auto& m : conv.members) {
            body << "gcm\t" << conv.id << "\t" << m.account << "\t"
                 << static_cast<int>(m.state) << "\n";
        }
    }
    for (const auto& [id, msg] : messages_) {
        body << "gmsg\t" << msg.id << "\t" << msg.conversation_id << "\t" << msg.sender << "\t"
             << msg.sent_at << "\t" << msg.edited_at << "\t" << static_cast<int>(msg.deletion) << "\t"
             << msg.deleted_at << "\t" << (msg.under_review ? 1 : 0) << "\t" << pct_encode(msg.text)
             << "\n";
    }
    for (const auto& [id, msg] : messages_) {
        for (const auto& h : msg.edit_history) {
            body << "gmh\t" << msg.id << "\t" << pct_encode(h) << "\n";
        }
    }
    const std::string data = body.str();

    const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        throw std::runtime_error(std::string("store open failed: ") + std::strerror(errno));
    }
    try {
        write_all(fd, data);
        if (::fsync(fd) != 0) {
            const int e = errno;
            throw std::runtime_error(std::string("store fsync failed: ") + std::strerror(e));
        }
    } catch (...) {
        ::close(fd);
        ::unlink(tmp.c_str());
        throw;
    }
    if (::close(fd) != 0) {
        ::unlink(tmp.c_str());
        throw std::runtime_error(std::string("store close failed: ") + std::strerror(errno));
    }
    if (::rename(tmp.c_str(), path_.c_str()) != 0) {
        const int e = errno;
        ::unlink(tmp.c_str());
        throw std::runtime_error(std::string("store rename failed: ") + std::strerror(e));
    }
}

std::optional<Avatar> FileStore::get(AccountId owner) const {
    auto it = avatars_.find(owner);
    if (it == avatars_.end()) return std::nullopt;
    return it->second;
}

void FileStore::set(const Avatar& avatar) {
    avatars_[avatar.owner] = avatar;
    flush();
}

void FileStore::erase(AccountId owner) {
    avatars_.erase(owner);
    flush();
}

ConversationId FileStore::create_conversation(const Conversation& conv) {
    const ConversationId id = next_conv_id_++;
    Conversation stored = conv;
    stored.id = id;
    conversations_[id] = stored;
    flush();
    return id;
}

std::optional<Conversation> FileStore::get_conversation(ConversationId id) const {
    auto it = conversations_.find(id);
    if (it == conversations_.end()) return std::nullopt;
    return it->second;
}

void FileStore::save_conversation(const Conversation& conv) {
    conversations_[conv.id] = conv;
    flush();
}

std::vector<Conversation> FileStore::conversations_for(AccountId account) const {
    std::vector<Conversation> out;
    for (const auto& [id, conv] : conversations_) {
        for (const auto& m : conv.members) {
            if (m.account == account) {
                out.push_back(conv);
                break;
            }
        }
    }
    return out;
}

MessageId FileStore::save_message(const Message& msg) {
    const MessageId id = next_message_id_++;
    Message stored = msg;
    stored.id = id;
    messages_[id] = stored;
    flush();
    return id;
}

std::optional<Message> FileStore::get_message(MessageId id) const {
    auto it = messages_.find(id);
    if (it == messages_.end()) return std::nullopt;
    return it->second;
}

void FileStore::update_message(const Message& msg) {
    messages_[msg.id] = msg;
    flush();
}

std::vector<Message> FileStore::all_messages() const {
    std::vector<Message> out;
    for (const auto& [id, msg] : messages_) out.push_back(msg);
    return out;
}

std::vector<Message> FileStore::messages_in(ConversationId conv_id) const {
    std::vector<Message> out;
    for (const auto& [id, msg] : messages_) {
        if (msg.conversation_id == conv_id) out.push_back(msg);
    }
    std::stable_sort(out.begin(), out.end(),
                      [](const Message& a, const Message& b) { return a.sent_at < b.sent_at; });
    return out;
}

bool FileStore::add_jukebox_track(const std::string& owner, const std::string& video_id) {
    if (!by_username_.count(owner)) return false;
    if (!valid_youtube_id(video_id)) return false;
    if (jukebox_[owner].size() >= kJukeboxQueueCap) return false;
    jukebox_[owner].push_back(JukeboxTrack{owner, video_id, now()});
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

std::vector<Avatar> FileStore::all_avatars() const {
    std::vector<Avatar> out;
    out.reserve(avatars_.size());
    for (const auto& [id, av] : avatars_) out.push_back(av);
    return out;
}

} // namespace liquidgov::platform
