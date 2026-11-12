// mail: 1:1 mail, mail read marks, attachment download names.
// LGP1 record types: m ma afn mlr (format: the header comment of file_store.hpp).
// Split out of file_store.cpp 2026-11-12 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"
#include "liquidgov/platform/clock.hpp"

#include <cstdint>
#include <optional>
#include <ostream>
#include <set>
#include <stdexcept>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

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

void FileStore::set_attachment_name(const std::string& key, const std::string& name) {
    attachment_names_[key] = name;
    flush();
}

std::optional<std::string> FileStore::attachment_name(const std::string& key) const {
    auto it = attachment_names_.find(key);
    if (it == attachment_names_.end()) return std::nullopt;
    return it->second;
}

// A name lives as long as its message: 1:1 mail still in mail_ (account
// deletion relabels from/to, so the key stops matching), a group message
// not deleted for everyone whose sender still exists.
void FileStore::prune_attachment_names() {
    std::set<std::string> live;
    for (const auto& m : mail_)
        if (m.attachment_blob_id) live.insert(attachment_name_key(m));
    for (const auto& [id, m] : messages_)
        if (!m.attachment_blob_id.empty() && m.deletion != DeletionState::DeletedForEveryone && accounts_.count(m.sender))
            live.insert(attachment_name_key(m));
    for (auto it = attachment_names_.begin(); it != attachment_names_.end();)
        it = live.count(it->first) ? std::next(it) : attachment_names_.erase(it);
}

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_mail(const std::vector<std::string>& f, LoadPending& pending) {
    (void)pending;
    const std::string& tag = f[0];
    if (tag == "m") {
        if (f.size() != 4 && f.size() != 5) throw std::runtime_error("LGP1: bad m record");
        MailMessage msg;
        msg.from = pct_decode(f[1]);
        msg.to = pct_decode(f[2]);
        msg.text = pct_decode(f[3]);
        msg.ts = (f.size() == 5) ? parse_i64(f[4], "mail ts") : 0;
        mail_.push_back(msg);
        return true;
    }
    if (tag == "ma") {
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
        return true;
    }
    if (tag == "afn") {
        // 4 fields: the first test build's (blob, sender) records, never on the live site; dropped
        if (f.size() == 4) return true;
        if (f.size() != 3) throw std::runtime_error("LGP1: bad afn record");
        attachment_names_[pct_decode(f[1])] = pct_decode(f[2]);
        return true;
    }
    if (tag == "mlr") {
        if (f.size() != 4) throw std::runtime_error("LGP1: bad mlr record");
        const std::string user = pct_decode(f[1]);
        const std::string key = pct_decode(f[2]);
        const auto ts = parse_i64(f[3], "mlr ts");
        if (!user.empty() && !key.empty()) mail_last_read_[user][key] = ts;
        return true;
    }
    return false;
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_mail(std::ostream& body) {
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
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_attachment_names(std::ostream& body) {
    for (const auto& [key, name] : attachment_names_) {
        body << "afn\t" << pct_encode(key) << "\t" << pct_encode(name) << "\n";
    }
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_mail_read(std::ostream& body) {
    for (const auto& [user, keys] : mail_last_read_) {
        if (!by_username_.count(user)) continue;
        for (const auto& [key, ts] : keys) {
            body << "mlr\t" << pct_encode(user) << "\t" << pct_encode(key) << "\t" << ts << "\n";
        }
    }
}

} // namespace liquidgov::platform
