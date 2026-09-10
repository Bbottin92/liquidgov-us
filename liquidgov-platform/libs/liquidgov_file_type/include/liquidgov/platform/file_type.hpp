#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Content-based type detection for file-transfer. Deliberately an
// allow-list, not a deny-list — the opposite posture from the CSS
// sanitizer, and for a specific reason: CSS's feature was expressive
// freedom against a small, enumerable set of known dangers. File types
// are the reverse shape — the space of dangerous formats isn't
// enumerable, so default-allow-except-known-bad would miss whatever
// wasn't anticipated. Detection is always by content (signature/magic
// bytes and, for container formats, actual structure), never by
// filename or claimed MIME type.

namespace liquidgov::platform {

enum class DetectedType {
    Unknown,
    Png,
    Jpeg,
    Gif,
    Pdf,
    PlainText,
    // A ZIP container that may or may not be a macro-free OOXML
    // document — see inspect_ooxml_container below. Detection alone
    // does not clear this type; the container inspection must also pass.
    OoxmlZipContainer,
    // ISO base media container (MP4/MOV/M4V) — matched on the "ftyp" box
    // at byte offset 4, regardless of the specific brand that follows.
    Mp4,
    // WebM/Matroska — matched on the EBML header magic.
    WebM,
};

DetectedType detect_type(const std::vector<std::uint8_t>& bytes);

struct OoxmlInspection {
    bool is_valid_zip = false;
    bool has_macro_content = false; // reject if true, regardless of what else is clean
    std::string rejection_reason;   // set only when has_macro_content is true
};

// Real container inspection, not a filename check. A single grep for
// "vbaProject.bin" is the check that has already failed historically —
// OOXML lets the macro part be named anything, provided the content
// type / relationship declarations say what it is. This inspects:
//   - [Content_Types].xml and .rels for VBA/oleObject/ActiveX/
//     attachedTemplate-class content types or relationship types
//   - every zip member path for "embeddings/"
//   - every zip member's own leading bytes for the OLE compound-file
//     signature (D0 CF 11 E0) — this is where a renamed vbaProject.bin
//     or oleObjectN.bin actually lives regardless of what it's called
//
// Known, stated limitation: content-type/relationship matching here is
// substring-based over the declarative XML, not a full XML-aware parse.
// That is a real, cheap first layer, not a claim of being unevadable by
// a sufficiently adversarial, deliberately obfuscated package.
OoxmlInspection inspect_ooxml_container(const std::vector<std::uint8_t>& zip_bytes);

} // namespace liquidgov::platform
