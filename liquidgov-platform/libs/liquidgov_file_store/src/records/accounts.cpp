// accounts: Accounts, sign-in sessions, phone fingerprints and quarantine, e-mail, reset tokens, Google ids, account deletion.
// LGP1 record types: a s ph pq ml pr gid em (format: the header comment of file_store.hpp).
// Split out of file_store.cpp 2026-11-12 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"
#include "liquidgov/platform/clock.hpp"

#include <sodium.h>

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

namespace {

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

bool FileStore::username_exists(const std::string& username) const {
    return by_username_.count(username) > 0;
}

std::size_t FileStore::account_count() const {
    return accounts_.size();
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
    locked_.erase(name);
    program_grants_.erase(name);
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
    map_prefs_.erase(name);
    pains_.erase(std::remove_if(pains_.begin(), pains_.end(), [&](const PainPoint& p) { return p.owner == name; }),
                 pains_.end());
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
    for (auto& r : proposal_replies_) {
        if (r.author == name) r.author = "[deleted account]";
    }
    for (auto& [pid, reactions] : proposal_reactions_) reactions.erase(name);
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
    here_choice_.erase(name);
    forget_styles_of(name);
    canvas_frames_.erase(id);
    canvas_timelapse_.erase(id);
    avatars_.erase(id);
    jukebox_.erase(name);
    jukebox_history_.erase(name);
    jukebox_autoplay_.erase(name);
    jukebox_relay_.erase(name);
    by_username_.erase(name);
    accounts_.erase(it);
    prune_attachment_names(); // the file names they sent or received (may hold personal details)
    flush();
    return true;
}

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_accounts(const std::vector<std::string>& f, LoadPending& pending) {
    (void)pending;
    const std::string& tag = f[0];
    if (tag == "a") {
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
        return true;
    }
    if (tag == "s") {
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
        return true;
    }
    if (tag == "ph") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad ph record");
        phone_fp_[parse_u64(f[1], "ph id")] = pct_decode(f[2]);
        return true;
    }
    if (tag == "pq") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad pq record");
        quarantine_[pct_decode(f[1])] = parse_i64(f[2], "pq until");
        return true;
    }
    if (tag == "ml") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad ml record");
        emails_[pct_decode(f[1])] = pct_decode(f[2]);
        return true;
    }
    if (tag == "pr") {
        if (f.size() != 4) throw std::runtime_error("LGP1: bad pr record");
        ResetToken t;
        t.token_hash = pct_decode(f[1]);
        t.until_unix = parse_i64(f[3], "pr until_unix");
        if (t.token_hash.empty()) throw std::runtime_error("LGP1: bad pr token_hash");
        reset_tokens_[pct_decode(f[2])] = t;
        return true;
    }
    if (tag == "gid") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad gid record");
        google_ids_[pct_decode(f[1])] = pct_decode(f[2]);
        return true;
    }
    if (tag == "em") {
        // Leftover from an abandoned schema that fingerprinted email
        // separately from prof -- nothing here writes this tag anymore
        // (prof carries its own email_fp field), but a store that still
        // has old lines from that schema must still boot.
        if (f.size() != 3) throw std::runtime_error("LGP1: bad em record");
        return true;
    }
    return false;
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_accounts(std::ostream& body) {
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
}

} // namespace liquidgov::platform
