#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "liquidgov/platform/blob.hpp"
#include "liquidgov/platform/file_type.hpp"
#include "liquidgov/platform/scanner.hpp"

// The messenger file-transfer ingest pipeline. Pipeline order, each step
// for a specific, already-established reason:
//   1. Byte-length check, before any parsing — a zip bomb or oversized
//      claimed-PNG should die on size, not inside a parser.
//   2. Content-based type detection (never filename/claimed MIME).
//   3. Type-specific structural proof: OOXML gets real container
//      inspection (macro/embedding/OLE-signature checks), not just an
//      allow because the prefix matched.
//   4. Normalize where possible (images: decode/re-encode). This is the
//      actual polyglot kill for that class, not a description of one.
//   5. Malware scan the bytes that will actually be stored — after
//      normalization, not the original upload, so scanning and storing
//      never diverge on what object they're each looking at.
//   6. Store as a content-addressed blob, shared with gallery/avatar.
//
// SVG and archives (beyond the OOXML container case) are deliberately
// off this list — SVG for the same script-injection-via-markup class
// already refused for raw CSS; general archives because the outer type
// says nothing about what's inside, and full recursive validation is a
// separate, larger project.
//
// Video (MP4/WebM) gets the same treatment as PDF/PlainText, not images:
// a real content-signature match (see file_type.hpp), then stored as-is.
// There is no re-encode step for video the way there is for images —
// transcoding is a separate, much larger project — so this is a stated,
// narrower guarantee than the image case: the container's outer magic is
// verified, not its full internal structure. The malware scan still runs
// against whatever gets stored, same as every other type here.

namespace liquidgov::platform {

enum class FileTransferError {
    None,
    TooLarge,
    TypeNotAllowed,
    ActiveContentBlocked, // macro, embedded object, or OLE signature found
    FailedScan,
};

struct FileTransferResult {
    FileTransferError error = FileTransferError::None;
    std::optional<BlobId> blob_id;
    std::optional<std::string> stored_mime_type;
};

// Per-type caps. These specific numbers are the same category of
// placeholder flagged throughout the design as needing real tuning
// against real usage, not treated as final here — protocol_max_bytes is
// the wire-level ceiling the resumable-upload design sizes against;
// per-type caps sit under it and can move independently without
// changing the protocol.
struct SizeCaps {
    std::uint64_t image_bytes = 10ull * 1024 * 1024;
    std::uint64_t document_bytes = 25ull * 1024 * 1024;
    std::uint64_t video_bytes = 50ull * 1024 * 1024;
    std::uint64_t protocol_max_bytes = 100ull * 1024 * 1024;
};

FileTransferResult ingest_file(BlobStore& blobs, MalwareScanner& scanner,
                                const SizeCaps& caps, const std::vector<std::uint8_t>& bytes);

} // namespace liquidgov::platform
