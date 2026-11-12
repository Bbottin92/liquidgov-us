#pragma once
// records/mail.hpp: the mail record family's types and helpers (moved out of file_store.hpp 2026-11-13, code
// moved, not rewritten). The LGP1 format of every record type stays in file_store.hpp's header comment.

#include "liquidgov/platform/messenger.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace liquidgov::platform {

struct MailMessage {
    std::string from;
    std::string to;
    std::string text;
    std::int64_t ts = 0;
    // Set only for messages carrying a file_transfer-ingested attachment.
    // blob_id/mime come straight out of FileTransferResult; the original
    // filename is deliberately not kept (avoids header-injection surface
    // and lets the store name the download itself from mime).
    std::optional<std::string> attachment_blob_id;
    std::optional<std::string> attachment_mime;
};

// The "afn" key of a message's attachment name: one per message, so the
// same bytes sent twice keep each send's own name. Usernames can't hold
// '|' (valid_username), so the key splits back unambiguously.
inline std::string attachment_name_key(const MailMessage& m) {
    return "d|" + m.from + "|" + m.to + "|" + std::to_string(m.ts) + "|" + m.attachment_blob_id.value_or("");
}

inline std::string attachment_name_key(const Message& m) { return "g|" + std::to_string(m.id); }

}  // namespace liquidgov::platform
