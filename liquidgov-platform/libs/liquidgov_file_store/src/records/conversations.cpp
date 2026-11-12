// conversations: Group conversations and their messages (ConversationStore).
// LGP1 record types: gc gcm gmsg gmh (format: the header comment of file_store.hpp).
// Split out of file_store.cpp 2026-11-12 (code moved, not rewritten). Listed in ../../sources.cmake.
#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace liquidgov::platform {

using namespace store_detail;

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

// The record types this module reads (one branch each, as they were in load()'s chain).
bool FileStore::read_conversations(const std::vector<std::string>& f, LoadPending& pending) {
    (void)pending;
    const std::string& tag = f[0];
    if (tag == "gc") {
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
        return true;
    }
    if (tag == "gcm") {
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
        return true;
    }
    if (tag == "gmsg") {
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
        return true;
    }
    if (tag == "gmh") {
        if (f.size() != 3) throw std::runtime_error("LGP1: bad gmh record");
        const auto msg_id = parse_u64(f[1], "gmh message id");
        auto it = messages_.find(msg_id);
        if (it == messages_.end()) throw std::runtime_error("LGP1: gmh with no message");
        it->second.edit_history.push_back(pct_decode(f[2]));
        return true;
    }
    return false;
}

// Save: this module's records, at their place in the store (flush() calls the sections in order).
void FileStore::write_conversations(std::ostream& body) {
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
}

} // namespace liquidgov::platform
