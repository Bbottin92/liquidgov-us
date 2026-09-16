#include "liquidgov/platform/file_store.hpp"

#include "liquidgov/platform/clock.hpp"

#include <sodium.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <random>
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

double parse_dbl(const std::string& s, const char* what) {
    if (s.empty()) throw std::runtime_error(std::string("LGP1: empty ") + what);
    std::size_t idx = 0;
    double v = 0;
    try {
        v = std::stod(s, &idx);
    } catch (...) {
        throw std::runtime_error(std::string("LGP1: bad ") + what);
    }
    if (idx != s.size() || !std::isfinite(v))
        throw std::runtime_error(std::string("LGP1: bad ") + what);
    return v;
}

std::string fmt_stored(double v) {
    std::ostringstream o;
    o.precision(12);
    o << v;
    return o.str();
}

bool valid_iso_date(const std::string& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (int i : {0, 1, 2, 3, 5, 6, 8, 9}) {
        if (s[static_cast<std::size_t>(i)] < '0' || s[static_cast<std::size_t>(i)] > '9') return false;
    }
    const int y = std::stoi(s.substr(0, 4));
    const int m = std::stoi(s.substr(5, 2));
    const int d = std::stoi(s.substr(8, 2));
    return y >= 2000 && y <= 2100 && m >= 1 && m <= 12 && d >= 1 && d <= 31;
}

bool spec_ok(const BallotSpec& s) {
    const int k = static_cast<int>(s.kind);
    if (k < 0 || k > 4) return false;
    if (s.unit.size() > 24) return false;
    if (s.kind == BallotKind::Choice) {
        if (s.options.size() < 2 || s.options.size() > 12) return false;
        for (const auto& o : s.options) {
            if (o.empty() || o.size() > 80) return false;
        }
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

std::size_t FileStore::account_count() const {
    return accounts_.size();
}

std::size_t FileStore::post_count() const {
    return posts_.size();
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

std::vector<PlatformAccount> FileStore::all_accounts() const {
    std::vector<PlatformAccount> out;
    out.reserve(accounts_.size());
    for (const auto& [id, acc] : accounts_) out.push_back(acc);
    std::sort(out.begin(), out.end(), [](const PlatformAccount& a, const PlatformAccount& b) {
        return a.created_at > b.created_at;
    });
    return out;
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

bool FileStore::set_password_hash(AccountId id, const std::string& password_hash) {
    auto it = accounts_.find(id);
    if (it == accounts_.end()) return false;
    if (password_hash.empty()) return false;
    it->second.password_hash = password_hash;
    flush();
    return true;
}

void FileStore::set_email(const std::string& username, const std::string& email) {
    // Keep the index a true 1:1: don't let a second username steal an
    // address already claimed by someone else.
    auto existing = username_by_email(email);
    if (existing && *existing != username) return;
    emails_[username] = email;
    flush();
}

std::optional<std::string> FileStore::email_of(const std::string& username) const {
    auto it = emails_.find(username);
    if (it == emails_.end()) return std::nullopt;
    return it->second;
}

std::optional<std::string> FileStore::username_by_email(const std::string& email) const {
    for (const auto& [username, addr] : emails_) {
        if (addr == email) return username;
    }
    return std::nullopt;
}

void FileStore::set_reset_token(const std::string& username, const std::string& token_hash,
                                 std::int64_t until_unix) {
    reset_tokens_[username] = ResetToken{token_hash, until_unix};
    flush();
}

std::optional<std::string> FileStore::consume_reset_token(const std::string& token_hash) {
    for (auto it = reset_tokens_.begin(); it != reset_tokens_.end(); ++it) {
        if (it->second.token_hash != token_hash) continue;
        const std::string username = it->first;
        const bool expired = it->second.until_unix < now();
        reset_tokens_.erase(it);
        flush();
        return expired ? std::nullopt : std::make_optional(username);
    }
    return std::nullopt;
}

void FileStore::set_google_id(const std::string& username, const std::string& google_sub) {
    google_ids_[username] = google_sub;
    flush();
}

std::optional<std::string> FileStore::username_by_google_sub(const std::string& google_sub) const {
    for (const auto& [username, sub] : google_ids_) {
        if (sub == google_sub) return username;
    }
    return std::nullopt;
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

std::vector<std::string> FileStore::mail_peers(const std::string& user) const {
    std::vector<std::string> out;
    std::set<std::string> seen;
    for (const auto& m : mail_) {
        std::string other;
        if (m.from == user) other = m.to;
        else if (m.to == user) other = m.from;
        else continue;
        if (other.empty() || seen.count(other)) continue;
        seen.insert(other);
        out.push_back(other);
    }
    return out;
}

std::int64_t FileStore::last_mail_ts(const std::string& a, const std::string& b) const {
    std::int64_t ts = 0;
    for (const auto& m : mail_) {
        if ((m.from == a && m.to == b) || (m.from == b && m.to == a)) {
            if (m.ts > ts) ts = m.ts;
        }
    }
    return ts;
}

std::string FileStore::last_mail_preview(const std::string& a, const std::string& b) const {
    const MailMessage* last = nullptr;
    for (const auto& m : mail_) {
        if ((m.from == a && m.to == b) || (m.from == b && m.to == a)) last = &m;
    }
    if (!last) return "";
    if (!last->text.empty()) return last->text;
    if (last->attachment_blob_id) return "(attachment)";
    return "";
}

void FileStore::mark_thread_read(const std::string& user, const std::string& thread_key) {
    if (user.empty() || thread_key.empty()) return;
    mail_last_read_[user][thread_key] = now();
    flush();
}

std::int64_t FileStore::thread_last_read(const std::string& user, const std::string& thread_key) const {
    auto uit = mail_last_read_.find(user);
    if (uit == mail_last_read_.end()) return 0;
    auto kit = uit->second.find(thread_key);
    if (kit == uit->second.end()) return 0;
    return kit->second;
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

void FileStore::block(const std::string& actor, const std::string& other, const std::string& reason) {
    if (actor.empty() || other.empty() || actor == other) return;
    unfriend(actor, other);
    blocks_.insert({actor, other});
    block_meta_[{actor, other}] = BlockEntry{actor, other, now(), reason};
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

std::vector<BlockEntry> FileStore::block_entries_of(const std::string& actor) const {
    std::vector<BlockEntry> out;
    for (const auto& p : blocks_) {
        if (p.first != actor) continue;
        auto it = block_meta_.find(p);
        out.push_back(it != block_meta_.end() ? it->second : BlockEntry{p.first, p.second, 0, ""});
    }
    return out;
}

void FileStore::unblock(const std::string& actor, const std::string& other) {
    blocks_.erase({actor, other});
    block_meta_.erase({actor, other});
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
        else if (filter == FeedFilter::Drawings) include = (personal || committee_post) && !it->attachment_blob_id.empty();
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

void FileStore::cast_ballot_value(std::uint64_t proposal_id, const std::string& voter, const std::string& payload) {
    if (voter.empty()) return;
    auto p = proposal_by_id(proposal_id);
    if (!p || p->ballot.kind == BallotKind::YesNo) return;
    if (!payload_ok(p->ballot, payload)) return;
    ballot_values_[proposal_id][voter] = payload;
    flush();
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
    t.n = static_cast<int>(pit->second.size());
    if (p->ballot.kind == BallotKind::Choice) {
        t.option_counts.assign(p->ballot.options.size(), 0);
        for (const auto& [voter, payload] : pit->second) {
            std::size_t idx = 0;
            unsigned long i = 0;
            try { i = std::stoul(payload, &idx); } catch (...) { continue; }
            if (idx == payload.size() && i < t.option_counts.size())
                ++t.option_counts[i];
        }
        return t;
    }
    if (p->ballot.kind == BallotKind::Date) {
        std::vector<std::string> dates;
        dates.reserve(pit->second.size());
        for (const auto& [voter, payload] : pit->second) dates.push_back(payload);
        std::sort(dates.begin(), dates.end());
        t.lo_date = dates.front();
        t.hi_date = dates.back();
        t.median_date = dates[(dates.size() - 1) / 2];
        return t;
    }
    std::vector<double> vals;
    vals.reserve(pit->second.size());
    for (const auto& [voter, payload] : pit->second) {
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

std::vector<std::string> FileStore::subcategories_of(CommunityCategory category) const {
    std::set<std::string> seen;
    for (const auto& [slug, c] : communities_) {
        if (c.category == category && !c.subcategory.empty()) seen.insert(c.subcategory);
    }
    return std::vector<std::string>(seen.begin(), seen.end());
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
    // Posts made into this room are left exactly as they are -- see this
    // method's own header comment. Closing a room is not a reason to erase
    // what anyone said in it.
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

int FileStore::unread_message_count(const std::string& user) const {
    int n = 0;
    for (const auto& note : notices_) {
        if (note.to == user && !note.read && note.type == "message") ++n;
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

void FileStore::mark_notices_read_of_type(const std::string& user, const std::string& type) {
    bool any = false;
    for (auto& note : notices_) {
        if (note.to == user && !note.read && note.type == type) {
            note.read = true;
            any = true;
        }
    }
    if (any) flush();
}

void FileStore::mark_notices_read_except_type(const std::string& user, const std::string& except_type) {
    bool any = false;
    for (auto& note : notices_) {
        if (note.to == user && !note.read && note.type != except_type) {
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

bool FileStore::is_locked(const std::string& username) const {
    return locked_.count(username) > 0;
}

void FileStore::set_locked(const std::string& username, bool locked) {
    if (locked) {
        locked_.insert(username);
        if (auto acc = find_by_username(username)) {
            for (const auto& s : sessions_for(acc->id)) revoke_session(s.id);
        }
    } else {
        locked_.erase(username);
    }
    flush();
}

void FileStore::add_audit_log(const std::string& operator_name, const std::string& action,
                              const std::string& target) {
    AuditLogEntry e;
    e.ts = now();
    e.operator_name = operator_name;
    e.action = action;
    e.target = target;
    audit_log_.push_back(std::move(e));
    if (audit_log_.size() > kAuditLogCap)
        audit_log_.erase(audit_log_.begin(), audit_log_.end() - static_cast<long>(kAuditLogCap));
    flush();
}

std::vector<AuditLogEntry> FileStore::recent_audit_log(std::size_t limit) const {
    std::vector<AuditLogEntry> out;
    for (auto it = audit_log_.rbegin(); it != audit_log_.rend() && out.size() < limit; ++it)
        out.push_back(*it);
    return out;
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
    locked_.erase(name);
    emails_.erase(name);
    reset_tokens_.erase(name);
    google_ids_.erase(name);
    std::set<std::pair<std::string, std::string>> keep_blk;
    for (const auto& p : blocks_) {
        if (p.first == name || p.second == name) { block_meta_.erase(p); continue; }
        keep_blk.insert(p);
    }
    blocks_.swap(keep_blk);
    std::set<std::pair<std::string, std::string>> keep_jsub;
    for (const auto& p : jukebox_subs_) {
        if (p.first == name || p.second == name) continue;
        keep_jsub.insert(p);
    }
    jukebox_subs_.swap(keep_jsub);
    prefs_.erase(name);
    for (auto& [slug, set] : members_) set.erase(name);
    std::vector<Post> keep_posts;
    for (auto& p : posts_) {
        if (p.author == name) p.author = "[deleted account]";
        if (p.repost_of_author == name) p.repost_of_author = "[deleted account]";
        keep_posts.push_back(p);
    }
    posts_.swap(keep_posts);
    for (auto& r : replies_) {
        if (r.author == name) r.author = "[deleted account]";
    }
    std::set<std::pair<std::uint64_t, std::string>> keep_seconds;
    for (const auto& s : seconds_) {
        if (s.second == name) continue;
        keep_seconds.insert(s);
    }
    seconds_.swap(keep_seconds);
    for (auto& p : proposals_) {
        if (p.author == name) p.author = "[deleted account]";
    }
    for (auto& [pid, votes] : proposal_votes_) votes.erase(name);
    for (auto& [pid, vals] : ballot_values_) vals.erase(name);
    std::vector<SessionId> drop;
    for (const auto& [sid, s] : sessions_) {
        if (s.account == id) drop.push_back(sid);
    }
    for (const auto& sid : drop) sessions_.erase(sid);
    phone_fp_.erase(id);
    blurbs_.erase(id);
    profiles_.erase(name);
    author_css_.erase(id);
    canvas_frames_.erase(id);
    canvas_timelapse_.erase(id);
    avatars_.erase(id);
    jukebox_.erase(name);
    jukebox_history_.erase(name);
    jukebox_autoplay_.erase(name);
    jukebox_relay_.erase(name);
    by_username_.erase(name);
    accounts_.erase(it);
    flush();
    return true;
}

void FileStore::load() {
    next_id_ = 1;
    next_post_id_ = 1;
    next_reply_id_ = 1;
    next_proposal_id_ = 1;
    next_note_id_ = 1;
    next_conv_id_ = 1;
    next_message_id_ = 1;
    by_username_.clear();
    accounts_.clear();
    sessions_.clear();
    blurbs_.clear();
    profiles_.clear();
    author_css_.clear();
    canvas_frames_.clear();
    canvas_timelapse_.clear();
    phone_fp_.clear();
    quarantine_.clear();
    emails_.clear();
    reset_tokens_.clear();
    google_ids_.clear();
    friends_.clear();
    requests_.clear();
    mail_.clear();
    blocks_.clear();
    block_meta_.clear();
    inner_.clear();
    prefs_.clear();
    posts_.clear();
    replies_.clear();
    seconds_.clear();
    proposals_.clear();
    proposal_votes_.clear();
    ballot_values_.clear();
    communities_.clear();
    members_.clear();
    notices_.clear();
    avatars_.clear();
    jukebox_.clear();
    jukebox_history_.clear();
    jukebox_subs_.clear();
    jukebox_autoplay_.clear();
    jukebox_relay_.clear();
    jukebox_pos_.clear();
    locked_.clear();
    audit_log_.clear();
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
    struct PendingBk {
        int kind = 0;
        std::string unit;
        double min_v = 0;
        double max_v = 0;
        double step = 1;
    };
    std::map<std::uint64_t, PendingBk> pending_bk;
    std::map<std::uint64_t, std::vector<std::pair<int, std::string>>> pending_bo;
    std::map<std::uint64_t, PostAudience> pending_audience;
    std::map<std::uint64_t, std::vector<std::string>> pending_exceptions;
    std::map<std::uint64_t, std::vector<std::string>> pending_friend_allowlist;
    std::map<std::uint64_t, std::vector<std::string>> pending_extra_groups;
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
        } else if (tag == "ml") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad ml record");
            emails_[pct_decode(f[1])] = pct_decode(f[2]);
        } else if (tag == "pr") {
            if (f.size() != 4) throw std::runtime_error("LGP1: bad pr record");
            ResetToken t;
            t.token_hash = pct_decode(f[1]);
            t.until_unix = parse_i64(f[3], "pr until_unix");
            if (t.token_hash.empty()) throw std::runtime_error("LGP1: bad pr token_hash");
            reset_tokens_[pct_decode(f[2])] = t;
        } else if (tag == "gid") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad gid record");
            google_ids_[pct_decode(f[1])] = pct_decode(f[2]);
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
        } else if (tag == "reply") {
            if (f.size() != 6) throw std::runtime_error("LGP1: bad reply record");
            PostReply r;
            r.id = parse_u64(f[1], "reply id");
            r.post_id = parse_u64(f[2], "reply post_id");
            r.author = pct_decode(f[3]);
            r.ts = parse_i64(f[4], "reply ts");
            r.text = pct_decode(f[5]);
            replies_.push_back(r);
            if (r.id >= next_reply_id_) next_reply_id_ = r.id + 1;
        } else if (tag == "repln") {
            if (f.size() != 2) throw std::runtime_error("LGP1: bad repln record");
            next_reply_id_ = parse_u64(f[1], "next_reply_id");
            if (next_reply_id_ == 0) next_reply_id_ = 1;
        } else if (tag == "sec") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad sec record");
            seconds_.insert({parse_u64(f[1], "sec post_id"), pct_decode(f[2])});
        } else if (tag == "prop") {
            if (f.size() != 6) throw std::runtime_error("LGP1: bad prop record");
            Proposal p;
            p.id = parse_u64(f[1], "proposal id");
            p.author = pct_decode(f[2]);
            p.ts = parse_i64(f[3], "proposal ts");
            p.title = pct_decode(f[4]);
            p.body = pct_decode(f[5]);
            proposals_.push_back(p);
            if (p.id >= next_proposal_id_) next_proposal_id_ = p.id + 1;
        } else if (tag == "propn") {
            if (f.size() != 2) throw std::runtime_error("LGP1: bad propn record");
            next_proposal_id_ = parse_u64(f[1], "next_proposal_id");
            if (next_proposal_id_ == 0) next_proposal_id_ = 1;
        } else if (tag == "pvote") {
            if (f.size() != 4) throw std::runtime_error("LGP1: bad pvote record");
            const auto pid = parse_u64(f[1], "pvote proposal id");
            const auto voter = pct_decode(f[2]);
            const auto cv = parse_u64(f[3], "pvote choice");
            if (cv > 2) throw std::runtime_error("LGP1: bad pvote choice");
            proposal_votes_[pid][voter] = static_cast<ProposalVote>(cv);
        } else if (tag == "pbk") {
            if (f.size() != 7) throw std::runtime_error("LGP1: bad pbk record");
            const auto pid = parse_u64(f[1], "pbk proposal id");
            const auto kind = parse_u64(f[2], "pbk kind");
            if (kind < 1 || kind > 4) throw std::runtime_error("LGP1: bad pbk kind");
            pending_bk[pid] = PendingBk{
                static_cast<int>(kind), pct_decode(f[3]),
                parse_dbl(f[4], "pbk min"), parse_dbl(f[5], "pbk max"),
                parse_dbl(f[6], "pbk step")
            };
        } else if (tag == "pbo") {
            if (f.size() != 4) throw std::runtime_error("LGP1: bad pbo record");
            const auto pid = parse_u64(f[1], "pbo proposal id");
            const auto idx = parse_u64(f[2], "pbo index");
            pending_bo[pid].push_back({static_cast<int>(idx), pct_decode(f[3])});
        } else if (tag == "pbv") {
            if (f.size() != 4) throw std::runtime_error("LGP1: bad pbv record");
            const auto pid = parse_u64(f[1], "pbv proposal id");
            const auto voter = pct_decode(f[2]);
            if (!voter.empty()) ballot_values_[pid][voter] = pct_decode(f[3]);
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
        } else if (tag == "pv") {
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
        } else if (tag == "pau") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad pau record");
            const auto av = parse_u64(f[2], "pau audience");
            if (av > static_cast<std::uint64_t>(PostAudience::GroupsOnly))
                throw std::runtime_error("LGP1: bad pau audience value");
            pending_audience[parse_u64(f[1], "pau post id")] = static_cast<PostAudience>(av);
        } else if (tag == "px") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad px record");
            pending_exceptions[parse_u64(f[1], "px post id")].push_back(pct_decode(f[2]));
        } else if (tag == "pf") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad pf record");
            pending_friend_allowlist[parse_u64(f[1], "pf post id")].push_back(pct_decode(f[2]));
        } else if (tag == "pg") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad pg record");
            pending_extra_groups[parse_u64(f[1], "pg post id")].push_back(pct_decode(f[2]));
        } else if (tag == "comm") {
            if (f.size() != 6 && f.size() != 8) throw std::runtime_error("LGP1: bad comm record");
            Community c;
            c.slug = pct_decode(f[1]);
            c.name = pct_decode(f[2]);
            c.owner = pct_decode(f[3]);
            c.about = pct_decode(f[4]);
            c.created_at = parse_i64(f[5], "comm created");
            if (f.size() == 8) {
                int cat = 10;
                try { cat = std::stoi(f[6]); } catch (...) { cat = 10; }
                if (cat < 0 || cat > 10) cat = 10;
                c.category = static_cast<CommunityCategory>(cat);
                c.subcategory = pct_decode(f[7]);
            }
            communities_[c.slug] = c;
        } else if (tag == "cm") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad cm record");
            members_[pct_decode(f[1])].insert(pct_decode(f[2]));
        } else if (tag == "blk") {
            if (f.size() != 3 && f.size() != 5) throw std::runtime_error("LGP1: bad blk record");
            BlockEntry b;
            b.actor = pct_decode(f[1]);
            b.other = pct_decode(f[2]);
            if (f.size() == 5) {
                b.at = parse_i64(f[3], "blk at");
                b.reason = pct_decode(f[4]);
            }
            blocks_.insert({b.actor, b.other});
            block_meta_[{b.actor, b.other}] = b;
        } else if (tag == "inner") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad inner record");
            inner_[pct_decode(f[1])].insert(pct_decode(f[2]));
        } else if (tag == "pref") {
            if (f.size() != 9 && f.size() != 12 && f.size() != 13 && f.size() != 14) throw std::runtime_error("LGP1: bad pref record");
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
            if (f.size() == 12 || f.size() == 13) {
                const auto jp = parse_u64(f[9], "jukebox_policy");
                if (jp > 2) throw std::runtime_error("LGP1: bad jukebox_policy");
                p.jukebox_policy = static_cast<JukeboxPolicy>(jp);
                const auto pv = parse_u64(f[10], "profile_visibility");
                const auto ov = parse_u64(f[11], "posts_visibility");
                if (pv > 3 || ov > 3) throw std::runtime_error("LGP1: bad visibility");
                p.profile_visibility = static_cast<Visibility>(pv);
                p.posts_visibility = static_cast<Visibility>(ov);
                if (f.size() == 13) p.intro_every_visit = parse_bit(f[12], "intro_every_visit");
            } else if (f.size() == 14) {
                // Superseded shape from this release's own earlier dev build:
                // jukebox_policy plus four independent hide_from_friends/others
                // bools. Translated to the closest point on the Visibility
                // ladder -- lossy only for the one combination that ladder
                // can't express (hidden from friends but not from strangers),
                // which nothing but this codebase's own throwaway test
                // accounts ever produced.
                const auto jp = parse_u64(f[9], "jukebox_policy");
                if (jp > 2) throw std::runtime_error("LGP1: bad jukebox_policy");
                p.jukebox_policy = static_cast<JukeboxPolicy>(jp);
                const bool hide_profile_friends = parse_bit(f[10], "hide_profile_from_friends");
                const bool hide_profile_others = parse_bit(f[11], "hide_profile_from_others");
                const bool hide_posts_friends = parse_bit(f[12], "hide_posts_from_friends");
                const bool hide_posts_others = parse_bit(f[13], "hide_posts_from_others");
                p.profile_visibility = hide_profile_friends ? Visibility::OnlyMe
                    : hide_profile_others ? Visibility::Friends : Visibility::Public;
                p.posts_visibility = hide_posts_friends ? Visibility::OnlyMe
                    : hide_posts_others ? Visibility::Friends : Visibility::Public;
            }
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
        } else if (tag == "jh") {
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
        } else if (tag == "jsub") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad jsub record");
            jukebox_subs_.insert({pct_decode(f[1]), pct_decode(f[2])});
        } else if (tag == "gcn") {
            if (f.size() != 2) throw std::runtime_error("LGP1: bad gcn record");
            next_conv_id_ = parse_u64(f[1], "next_conversation_id");
            if (next_conv_id_ == 0) next_conv_id_ = 1;
        } else if (tag == "gmsn") {
            if (f.size() != 2) throw std::runtime_error("LGP1: bad gmsn record");
            next_message_id_ = parse_u64(f[1], "next_message_id");
            if (next_message_id_ == 0) next_message_id_ = 1;
        } else if (tag == "gc") {
            if (f.size() != 4 && f.size() != 5) throw std::runtime_error("LGP1: bad gc record");
            Conversation conv;
            conv.id = parse_u64(f[1], "conversation id");
            conv.is_group = parse_bit(f[2], "conversation is_group");
            conv.created_at = parse_i64(f[3], "conversation created_at");
            if (f.size() == 5) conv.title = pct_decode(f[4]);
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
            if (f.size() != 10 && f.size() != 12) throw std::runtime_error("LGP1: bad gmsg record");
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
            if (f.size() == 12) {
                msg.attachment_blob_id = pct_decode(f[10]);
                msg.attachment_mime = pct_decode(f[11]);
            }
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
        } else if (tag == "prof") {
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
        } else if (tag == "em") {
            // Leftover from an abandoned schema that fingerprinted email
            // separately from prof -- nothing here writes this tag anymore
            // (prof carries its own email_fp field), but a store that still
            // has old lines from that schema must still boot.
            if (f.size() != 3) throw std::runtime_error("LGP1: bad em record");
        } else if (tag == "jauto") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad jauto record");
            jukebox_autoplay_[pct_decode(f[1])] = parse_bit(f[2], "jauto");
        } else if (tag == "jrelay") {
            if (f.size() != 3) throw std::runtime_error("LGP1: bad jrelay record");
            auto target = pct_decode(f[2]);
            if (!target.empty()) jukebox_relay_[pct_decode(f[1])] = target;
        } else if (tag == "jpos") {
            if (f.size() != 5) throw std::runtime_error("LGP1: bad jpos record");
            JukeboxPos p;
            p.video_id = pct_decode(f[2]);
            p.offset_sec = parse_i64(f[3], "jpos offset_sec");
            p.updated_at = parse_i64(f[4], "jpos updated_at");
            if (!valid_youtube_id(p.video_id)) throw std::runtime_error("LGP1: bad jpos video_id");
            jukebox_pos_[pct_decode(f[1])] = p;
        } else if (tag == "lk") {
            if (f.size() != 2) throw std::runtime_error("LGP1: bad lk record");
            locked_.insert(pct_decode(f[1]));
        } else if (tag == "alog") {
            if (f.size() != 5) throw std::runtime_error("LGP1: bad alog record");
            AuditLogEntry e;
            e.ts = parse_i64(f[1], "alog ts");
            e.operator_name = pct_decode(f[2]);
            e.action = pct_decode(f[3]);
            e.target = pct_decode(f[4]);
            audit_log_.push_back(std::move(e));
        } else if (tag == "mlr") {
            if (f.size() != 4) throw std::runtime_error("LGP1: bad mlr record");
            const std::string user = pct_decode(f[1]);
            const std::string key = pct_decode(f[2]);
            const auto ts = parse_i64(f[3], "mlr ts");
            if (!user.empty() && !key.empty()) mail_last_read_[user][key] = ts;
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
        auto pauit = pending_audience.find(p.id);
        if (pauit != pending_audience.end()) p.audience = pauit->second;
        auto pxit = pending_exceptions.find(p.id);
        if (pxit != pending_exceptions.end()) p.exceptions = pxit->second;
        auto pfit = pending_friend_allowlist.find(p.id);
        if (pfit != pending_friend_allowlist.end()) p.friend_allowlist = pfit->second;
        auto pgit = pending_extra_groups.find(p.id);
        if (pgit != pending_extra_groups.end()) p.extra_groups = pgit->second;
    }

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
            for (const auto& item : opts) p.ballot.options.push_back(item.second);
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
    for (const auto& [username, email] : emails_) {
        if (!by_username_.count(username)) continue;
        body << "ml\t" << pct_encode(username) << "\t" << pct_encode(email) << "\n";
    }
    for (const auto& [username, tok] : reset_tokens_) {
        if (!by_username_.count(username)) continue;
        if (tok.until_unix <= now()) continue; // expired, never used -- drop rather than carry forward
        body << "pr\t" << pct_encode(tok.token_hash) << "\t" << pct_encode(username) << "\t"
             << tok.until_unix << "\n";
    }
    for (const auto& [username, sub] : google_ids_) {
        if (!by_username_.count(username)) continue;
        body << "gid\t" << pct_encode(username) << "\t" << pct_encode(sub) << "\n";
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
    body << "propn\t" << next_proposal_id_ << "\n";
    for (const auto& p : proposals_) {
        body << "prop\t" << p.id << "\t" << pct_encode(p.author) << "\t" << p.ts << "\t"
             << pct_encode(p.title) << "\t" << pct_encode(p.body) << "\n";
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
    }
    for (const auto& [pid, vals] : ballot_values_) {
        for (const auto& [voter, payload] : vals) {
            body << "pbv\t" << pid << "\t" << pct_encode(voter) << "\t" << pct_encode(payload) << "\n";
        }
    }
    for (const auto& [slug, c] : communities_) {
        body << "comm\t" << pct_encode(c.slug) << "\t" << pct_encode(c.name) << "\t"
             << pct_encode(c.owner) << "\t" << pct_encode(c.about) << "\t" << c.created_at << "\t"
             << static_cast<int>(c.category) << "\t" << pct_encode(c.subcategory) << "\n";
    }
    for (const auto& [slug, set] : members_) {
        for (const auto& u : set) {
            body << "cm\t" << pct_encode(slug) << "\t" << pct_encode(u) << "\n";
        }
    }
    for (const auto& b : blocks_) {
        auto mit = block_meta_.find(b);
        const BlockEntry meta = mit != block_meta_.end() ? mit->second : BlockEntry{b.first, b.second, 0, ""};
        body << "blk\t" << pct_encode(b.first) << "\t" << pct_encode(b.second) << "\t"
             << meta.at << "\t" << pct_encode(meta.reason) << "\n";
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
             << (p.notify_message ? 1 : 0) << "\t" << (p.notify_group ? 1 : 0) << "\t"
             << static_cast<int>(p.jukebox_policy) << "\t" << static_cast<int>(p.profile_visibility) << "\t"
             << static_cast<int>(p.posts_visibility) << "\t" << (p.intro_every_visit ? 1 : 0) << "\n";
    }
    for (const auto& [name, p] : profiles_) {
        body << "prof\t" << pct_encode(name) << "\t" << pct_encode(p.email_fp) << "\t"
             << pct_encode(p.display_name) << "\t" << pct_encode(p.hometown) << "\t"
             << pct_encode(p.living) << "\t" << pct_encode(p.interests) << "\t"
             << pct_encode(p.career) << "\t" << pct_encode(p.school) << "\n";
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
    for (const auto& name : locked_) {
        if (!by_username_.count(name)) continue;
        body << "lk\t" << pct_encode(name) << "\n";
    }
    {
        std::size_t start = audit_log_.size() > kAuditLogCap ? audit_log_.size() - kAuditLogCap : 0;
        for (std::size_t i = start; i < audit_log_.size(); ++i) {
            const auto& e = audit_log_[i];
            body << "alog\t" << e.ts << "\t" << pct_encode(e.operator_name) << "\t"
                 << pct_encode(e.action) << "\t" << pct_encode(e.target) << "\n";
        }
    }
    // Conversations/messages are never pruned when an account is deleted —
    // same "keep the row, relabel at render time" choice as mail_/posts_
    // above. The numeric sender/member id just stops resolving through
    // find_by_id; the site layer renders that as "[deleted account]".
    for (const auto& [id, conv] : conversations_) {
        body << "gc\t" << conv.id << "\t" << (conv.is_group ? 1 : 0) << "\t" << conv.created_at;
        if (!conv.title.empty()) body << "\t" << pct_encode(conv.title);
        body << "\n";
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
             << msg.deleted_at << "\t" << (msg.under_review ? 1 : 0) << "\t" << pct_encode(msg.text);
        if (!msg.attachment_blob_id.empty()) {
            body << "\t" << pct_encode(msg.attachment_blob_id) << "\t" << pct_encode(msg.attachment_mime);
        }
        body << "\n";
    }
    for (const auto& [id, msg] : messages_) {
        for (const auto& h : msg.edit_history) {
            body << "gmh\t" << msg.id << "\t" << pct_encode(h) << "\n";
        }
    }
    for (const auto& [user, keys] : mail_last_read_) {
        if (!by_username_.count(user)) continue;
        for (const auto& [key, ts] : keys) {
            body << "mlr\t" << pct_encode(user) << "\t" << pct_encode(key) << "\t" << ts << "\n";
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

std::int64_t FileStore::last_group_ts(ConversationId conv_id) const {
    std::int64_t ts = 0;
    auto cit = conversations_.find(conv_id);
    if (cit != conversations_.end()) ts = cit->second.created_at;
    for (const auto& [id, msg] : messages_) {
        if (msg.conversation_id == conv_id && msg.sent_at > ts) ts = msg.sent_at;
    }
    return ts;
}

std::string FileStore::last_group_preview(ConversationId conv_id) const {
    const Message* last = nullptr;
    for (const auto& [id, msg] : messages_) {
        if (msg.conversation_id != conv_id) continue;
        if (msg.deletion == DeletionState::DeletedForEveryone) continue;
        if (!last || msg.sent_at >= last->sent_at) last = &msg;
    }
    if (!last) return "";
    if (last->deletion == DeletionState::DeletedForEveryone) return "";
    if (!last->text.empty()) return last->text;
    if (!last->attachment_blob_id.empty()) return "(attachment)";
    return "";
}

std::optional<Message> FileStore::find_group_attachment(AccountId account, const std::string& blob_id) const {
    if (blob_id.empty()) return std::nullopt;
    for (const auto& [id, msg] : messages_) {
        if (msg.attachment_blob_id != blob_id) continue;
        auto cit = conversations_.find(msg.conversation_id);
        if (cit == conversations_.end()) continue;
        for (const auto& m : cit->second.members) {
            if (m.account == account) return msg;
        }
    }
    return std::nullopt;
}

std::vector<Message> FileStore::all_group_messages_with_attachments() const {
    std::vector<Message> out;
    for (const auto& [id, msg] : messages_) {
        if (!msg.attachment_blob_id.empty()) out.push_back(msg);
    }
    return out;
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

std::vector<Avatar> FileStore::all_avatars() const {
    std::vector<Avatar> out;
    out.reserve(avatars_.size());
    for (const auto& [id, av] : avatars_) out.push_back(av);
    return out;
}

} // namespace liquidgov::platform
