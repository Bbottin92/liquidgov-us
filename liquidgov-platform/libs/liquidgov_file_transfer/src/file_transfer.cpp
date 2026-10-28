#include "liquidgov/platform/file_transfer.hpp"

#include "liquidgov/platform/image_normalize.hpp"

namespace liquidgov::platform {

FileTransferResult ingest_file(BlobStore& blobs, MalwareScanner& scanner,
                                const SizeCaps& caps, const std::vector<std::uint8_t>& bytes,
                                const ProgramPolicy* programs) {
    // 1. Length first, before any parsing at all.
    if (bytes.size() > caps.protocol_max_bytes) {
        return {FileTransferError::TooLarge, std::nullopt, std::nullopt};
    }

    // 2. Content-based detection.
    DetectedType type = detect_type(bytes);

    // Programs: the sender's policy decides, then the scan; stored from
    // `bytes` directly (no copy -- these are the biggest uploads there are).
    if (const ProgramKindInfo* program = program_kind_of(type)) {
        if (!programs || (programs->kinds & static_cast<std::uint32_t>(program->kind)) == 0) {
            return {FileTransferError::ProgramNotAllowed, std::nullopt, std::nullopt};
        }
        if (bytes.size() > programs->max_bytes) {
            return {FileTransferError::TooLarge, std::nullopt, std::nullopt};
        }
        if (!scanner.is_clean(bytes)) {
            return {FileTransferError::FailedScan, std::nullopt, std::nullopt};
        }
        BlobId id = blobs.put(bytes, program->mime);
        return {FileTransferError::None, id, std::string(program->mime)};
    }

    std::vector<std::uint8_t> to_store;
    std::string mime;

    switch (type) {
        case DetectedType::Png:
        case DetectedType::Jpeg:
        case DetectedType::Gif: {
            if (bytes.size() > caps.image_bytes) {
                return {FileTransferError::TooLarge, std::nullopt, std::nullopt};
            }
            // The actual polyglot kill for the image class: if it will
            // not decode as exactly one real codec, this rejects rather
            // than guessing which interpretation was intended.
            auto normalized = normalize_image(bytes);
            if (!normalized.ok) {
                return {FileTransferError::TypeNotAllowed, std::nullopt, std::nullopt};
            }
            to_store = std::move(normalized.png_bytes);
            mime = "image/png";
            break;
        }

        case DetectedType::Pdf:
        case DetectedType::PlainText: {
            if (bytes.size() > caps.document_bytes) {
                return {FileTransferError::TooLarge, std::nullopt, std::nullopt};
            }
            // No re-encode is possible for these formats the way it is
            // for images — the allow-list gate plus the malware scan
            // are the defense here, not a normalize step.
            to_store = bytes;
            mime = (type == DetectedType::Pdf) ? "application/pdf"
                                                : "text/plain; charset=utf-8";
            break;
        }

        case DetectedType::Mp4:
        case DetectedType::WebM: {
            if (bytes.size() > caps.video_bytes) {
                return {FileTransferError::TooLarge, std::nullopt, std::nullopt};
            }
            // No re-encode/transcode attempted -- same posture as
            // PDF/PlainText below: the content-signature match plus the
            // malware scan are the defense here, not a normalize step.
            to_store = bytes;
            mime = (type == DetectedType::Mp4) ? "video/mp4" : "video/webm";
            break;
        }

        case DetectedType::OoxmlZipContainer: {
            if (bytes.size() > caps.document_bytes) {
                return {FileTransferError::TooLarge, std::nullopt, std::nullopt};
            }
            auto inspection = inspect_ooxml_container(bytes);
            if (!inspection.is_valid_zip || inspection.has_macro_content) {
                return {FileTransferError::ActiveContentBlocked, std::nullopt, std::nullopt};
            }
            to_store = bytes;
            mime = "application/vnd.openxmlformats-officedocument";
            break;
        }

        case DetectedType::Unknown:
        default:
            return {FileTransferError::TypeNotAllowed, std::nullopt, std::nullopt};
    }

    // Scan the bytes that will actually be stored, after normalization —
    // not the original upload. For the image case these genuinely
    // differ; scanning the wrong one would mean the stored object was
    // never actually the one checked.
    if (!scanner.is_clean(to_store)) {
        return {FileTransferError::FailedScan, std::nullopt, std::nullopt};
    }

    BlobId id = blobs.put(to_store, mime);
    return {FileTransferError::None, id, mime};
}

} // namespace liquidgov::platform
