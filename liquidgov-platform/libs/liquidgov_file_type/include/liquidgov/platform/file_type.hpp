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
    // Only a zip with a [Content_Types].xml part (every OOXML package has
    // one) is this type; any other zip is Zip, a program kind, so a plain
    // archive can't pass as a Word file.
    OoxmlZipContainer,
    // ISO base media container (MP4/MOV/M4V) — matched on the "ftyp" box
    // at byte offset 4, regardless of the specific brand that follows.
    Mp4,
    // WebM/Matroska — matched on the EBML header magic.
    WebM,
    // Programs: archives, installers and executables. Not on the
    // allow-list for anyone by default -- ingest_file takes them only
    // from a sender an operator allowed (ProgramPolicy, file_transfer.hpp),
    // because the outer type says nothing about what runs inside.
    Zip,         // a real zip (central directory parses) that isn't OOXML or an APK
    SevenZip,    // "7z\xBC\xAF\x27\x1C"
    Rar,         // "Rar!\x1A\x07"
    WindowsExe,  // "MZ" with a "PE\0\0" header where e_lfanew points (.exe/.dll)
    WindowsMsi,  // OLE compound file whose root CLSID is the Windows Installer one
    AndroidApk,  // a real zip holding AndroidManifest.xml and classes.dex
    LinuxElf,    // "\x7F" "ELF"
};

// The program kinds an operator can allow per person, as bits so a grant
// stores one number. Slugs are what the admin form and the store use.
enum class ProgramKind : std::uint32_t {
    Zip = 1u << 0,
    SevenZip = 1u << 1,
    Rar = 1u << 2,
    WindowsExe = 1u << 3,
    WindowsMsi = 1u << 4,
    AndroidApk = 1u << 5,
    LinuxElf = 1u << 6,
};
constexpr std::uint32_t kAllProgramKinds = (1u << 7) - 1;

struct ProgramKindInfo {
    ProgramKind kind;
    const char* slug;       // "zip", "7z", ... (admin form field names, store)
    const char* label;      // "Zip archive (.zip)"
    const char* extension;  // ".zip" -- forced onto a download's name
    const char* mime;       // stored Content-Type
};
// Every program kind, in the order the admin form lists them.
const std::vector<ProgramKindInfo>& program_kinds();
// The program kind a detected type is, or nullptr for a non-program type.
const ProgramKindInfo* program_kind_of(DetectedType type);
const ProgramKindInfo* program_kind_by_mime(const std::string& mime);

// Program magic (exe, ELF, 7z, RAR, msi, zip/apk) is checked before every
// other type, so a program can't pass as a video or an image.
DetectedType detect_type(const std::vector<std::uint8_t>& bytes);

// An archive hidden inside a file that is otherwise stored as-is (PDF,
// text, video): a zip central directory that parses (zip readers find it
// from the end), or a 7-Zip / RAR signature anywhere (their readers skip
// what's in front). Returns Zip / SevenZip / Rar, or Unknown when clean.
// ingest_file treats such a file as that program kind.
DetectedType embedded_archive(const std::vector<std::uint8_t>& bytes);

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
