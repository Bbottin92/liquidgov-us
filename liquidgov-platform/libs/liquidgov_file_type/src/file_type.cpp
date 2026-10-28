#include "liquidgov/platform/file_type.hpp"

#include "miniz.h"
#include "miniz_zip.h"

#include <algorithm>
#include <cstring>

namespace liquidgov::platform {

namespace {

bool starts_with(const std::vector<std::uint8_t>& bytes, std::initializer_list<int> sig) {
    if (bytes.size() < sig.size()) return false;
    std::size_t i = 0;
    for (int b : sig) {
        if (bytes[i++] != static_cast<std::uint8_t>(b)) return false;
    }
    return true;
}

bool looks_like_plain_text(const std::vector<std::uint8_t>& bytes) {
    if (bytes.empty()) return false;
    std::size_t sample = std::min<std::size_t>(bytes.size(), 8192);
    for (std::size_t i = 0; i < sample; i++) {
        std::uint8_t b = bytes[i];
        // Null bytes are the classic "this is binary" signal. Reject
        // outright rather than trying to guess an encoding.
        if (b == 0x00) return false;
        bool printable_or_whitespace =
            (b >= 0x20 && b <= 0x7E) || b == '\n' || b == '\r' || b == '\t' || b >= 0x80;
        if (!printable_or_whitespace) return false;
    }
    return true;
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

// The specific content-type/relationship-type strings that mean "this
// part is VBA, an OLE object, ActiveX, or a remote template" — checked
// against the declarative XML content, not against any zip member's
// filename.
bool contains_dangerous_declaration(const std::string& xml_lowercase) {
    static const char* kDangerousMarkers[] = {
        "vbaproject",
        "macroenabled",
        "oleobject",
        "activex",
        "attachedtemplate",
        "package/2006/relationships/oleobject",
    };
    for (const char* marker : kDangerousMarkers) {
        if (xml_lowercase.find(marker) != std::string::npos) return true;
    }
    return false;
}

std::uint32_t read_u32_le(const std::vector<std::uint8_t>& bytes, std::size_t at) {
    return static_cast<std::uint32_t>(bytes[at]) | (static_cast<std::uint32_t>(bytes[at + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[at + 2]) << 16) | (static_cast<std::uint32_t>(bytes[at + 3]) << 24);
}

// "MZ" alone is two printable letters; a real PE file also has "PE\0\0"
// at the offset its DOS header's e_lfanew field (0x3C) names.
bool is_pe_executable(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < 0x40 || bytes[0] != 'M' || bytes[1] != 'Z') return false;
    const std::uint64_t pe = read_u32_le(bytes, 0x3C);
    if (pe + 4 > bytes.size()) return false;
    return bytes[pe] == 'P' && bytes[pe + 1] == 'E' && bytes[pe + 2] == 0 && bytes[pe + 3] == 0;
}

// OLE compound files are also legacy .doc/.xls (still refused); a
// Windows Installer package is the one whose root storage carries the
// installer CLSID {000C1084-0000-0000-C000-000000000046}. The root entry
// is the first entry of the first directory sector the header names.
bool is_msi(const std::vector<std::uint8_t>& bytes) {
    if (!starts_with(bytes, {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1}) || bytes.size() < 512) return false;
    const unsigned shift = static_cast<unsigned>(bytes[0x1E]) | (static_cast<unsigned>(bytes[0x1F]) << 8);
    if (shift != 9 && shift != 12) return false;
    const std::uint64_t dir_sector = read_u32_le(bytes, 0x30);
    const std::uint64_t root = (dir_sector + 1) << shift;
    static const std::uint8_t kMsiClsid[16] = {0x84, 0x10, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00,
                                               0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};
    if (root + 0x50 + 16 > bytes.size()) return false;
    return std::memcmp(bytes.data() + root + 0x50, kMsiClsid, 16) == 0;
}

bool zip_has(mz_zip_archive& zip, const char* name) {
    return mz_zip_reader_locate_file(&zip, name, nullptr, 0) >= 0;
}

const std::vector<ProgramKindInfo> kProgramKinds = {
    {ProgramKind::Zip, "zip", "Zip archive (.zip)", ".zip", "application/zip"},
    {ProgramKind::SevenZip, "7z", "7-Zip archive (.7z)", ".7z", "application/x-7z-compressed"},
    {ProgramKind::Rar, "rar", "RAR archive (.rar)", ".rar", "application/vnd.rar"},
    {ProgramKind::WindowsExe, "exe", "Windows program (.exe)", ".exe", "application/vnd.microsoft.portable-executable"},
    {ProgramKind::WindowsMsi, "msi", "Windows installer (.msi)", ".msi", "application/x-msi"},
    {ProgramKind::AndroidApk, "apk", "Android app (.apk)", ".apk", "application/vnd.android.package-archive"},
    {ProgramKind::LinuxElf, "elf", "Linux program (no extension)", "", "application/x-elf"},
};

} // namespace

const std::vector<ProgramKindInfo>& program_kinds() { return kProgramKinds; }

const ProgramKindInfo* program_kind_of(DetectedType type) {
    ProgramKind kind;
    switch (type) {
        case DetectedType::Zip: kind = ProgramKind::Zip; break;
        case DetectedType::SevenZip: kind = ProgramKind::SevenZip; break;
        case DetectedType::Rar: kind = ProgramKind::Rar; break;
        case DetectedType::WindowsExe: kind = ProgramKind::WindowsExe; break;
        case DetectedType::WindowsMsi: kind = ProgramKind::WindowsMsi; break;
        case DetectedType::AndroidApk: kind = ProgramKind::AndroidApk; break;
        case DetectedType::LinuxElf: kind = ProgramKind::LinuxElf; break;
        default: return nullptr;
    }
    for (const auto& k : kProgramKinds)
        if (k.kind == kind) return &k;
    return nullptr;
}

const ProgramKindInfo* program_kind_by_mime(const std::string& mime) {
    for (const auto& k : kProgramKinds)
        if (mime == k.mime) return &k;
    return nullptr;
}

DetectedType detect_type(const std::vector<std::uint8_t>& bytes) {
    if (starts_with(bytes, {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A})) return DetectedType::Png;
    if (starts_with(bytes, {0xFF, 0xD8, 0xFF})) return DetectedType::Jpeg;
    if (starts_with(bytes, {'G', 'I', 'F', '8'})) return DetectedType::Gif;
    // ISO base media (MP4/MOV/M4V): a 4-byte box-size field then the
    // literal box type "ftyp" at offset 4 -- true for every brand
    // (isom/mp42/qt  /avc1/...), so this checks the box type only, not
    // any specific brand string.
    if (bytes.size() >= 8 && bytes[4] == 'f' && bytes[5] == 't' && bytes[6] == 'y' && bytes[7] == 'p')
        return DetectedType::Mp4;
    if (starts_with(bytes, {0x1A, 0x45, 0xDF, 0xA3})) return DetectedType::WebM;
    // Programs are matched on their leading magic before the PDF search
    // below, which looks anywhere in the first 1KB: an .exe or archive
    // with "%PDF" in its header must not pass as a PDF.
    if (starts_with(bytes, {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C})) return DetectedType::SevenZip;
    if (starts_with(bytes, {'R', 'a', 'r', '!', 0x1A, 0x07})) return DetectedType::Rar;
    if (starts_with(bytes, {0x7F, 'E', 'L', 'F'})) return DetectedType::LinuxElf;
    if (is_pe_executable(bytes)) return DetectedType::WindowsExe;
    if (is_msi(bytes)) return DetectedType::WindowsMsi;
    if (starts_with(bytes, {'P', 'K', 0x03, 0x04})) {
        // Prefix match alone is not proof — verify with a real zip
        // reader that actually parses the central directory (located at
        // the end of the file, not the start). A prefix-only check is
        // exactly what a polyglot is built to pass while failing this
        // real structural parse.
        mz_zip_archive zip{};
        if (mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0)) {
            DetectedType type = DetectedType::Zip;
            if (zip_has(zip, "[Content_Types].xml")) type = DetectedType::OoxmlZipContainer;
            else if (zip_has(zip, "AndroidManifest.xml") && zip_has(zip, "classes.dex")) type = DetectedType::AndroidApk;
            mz_zip_reader_end(&zip);
            return type;
        }
        return DetectedType::Unknown; // looked like a zip, did not actually parse as one
    }
    // PDF magic is allowed within the first 1KB, not only at offset 0 —
    // some real-world PDFs are preceded by junk bytes some tools insert.
    {
        std::size_t search_window = std::min<std::size_t>(bytes.size(), 1024);
        for (std::size_t i = 0; i + 4 <= search_window; i++) {
            if (bytes[i] == '%' && bytes[i + 1] == 'P' && bytes[i + 2] == 'D' &&
                bytes[i + 3] == 'F') {
                return DetectedType::Pdf;
            }
        }
    }
    if (looks_like_plain_text(bytes)) return DetectedType::PlainText;
    return DetectedType::Unknown;
}

OoxmlInspection inspect_ooxml_container(const std::vector<std::uint8_t>& zip_bytes) {
    OoxmlInspection result;

    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, zip_bytes.data(), zip_bytes.size(), 0)) {
        return result; // is_valid_zip stays false
    }
    result.is_valid_zip = true;

    mz_uint file_count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;

        std::string entry_name(stat.m_filename);

        // Any embedded-object path is rejected outright, independent of
        // the content-type check below.
        if (entry_name.find("embeddings/") != std::string::npos) {
            result.has_macro_content = true;
            result.rejection_reason = "embedded object at " + entry_name;
            mz_zip_reader_end(&zip);
            return result;
        }

        // Inspect declarative XML parts (content types, relationships)
        // for VBA/OLE/ActiveX/remote-template declarations.
        std::string lower_name = to_lower(entry_name);
        bool is_declaration_part =
            lower_name.find("[content_types].xml") != std::string::npos ||
            lower_name.find(".rels") != std::string::npos;

        if (is_declaration_part && stat.m_uncomp_size > 0 &&
            stat.m_uncomp_size < 5 * 1024 * 1024) { // sane bound before allocating
            std::vector<char> buf(stat.m_uncomp_size);
            if (mz_zip_reader_extract_to_mem(&zip, i, buf.data(), buf.size(), 0)) {
                std::string content(buf.begin(), buf.end());
                if (contains_dangerous_declaration(to_lower(content))) {
                    result.has_macro_content = true;
                    result.rejection_reason = "dangerous declaration in " + entry_name;
                    mz_zip_reader_end(&zip);
                    return result;
                }
            }
        }

        // Content-based check independent of the entry's declared name:
        // does this member's own data start with the OLE compound-file
        // signature? This is where a renamed vbaProject.bin or
        // oleObjectN.bin actually lives regardless of what the package
        // calls it. extract_to_mem requires a buffer sized for the full
        // entry, not a partial read, so this extracts the whole (small)
        // member and checks its head rather than trying to read just 8
        // bytes directly.
        if (stat.m_uncomp_size >= 8 && stat.m_uncomp_size < 20 * 1024 * 1024) {
            std::vector<char> full(stat.m_uncomp_size);
            if (mz_zip_reader_extract_to_mem(&zip, i, full.data(), full.size(), 0)) {
                static const unsigned char kOleSignature[] = {0xD0, 0xCF, 0x11, 0xE0,
                                                                0xA1, 0xB1, 0x1A, 0xE1};
                if (std::memcmp(full.data(), kOleSignature, 8) == 0) {
                    result.has_macro_content = true;
                    result.rejection_reason = "OLE compound file at " + entry_name;
                    mz_zip_reader_end(&zip);
                    return result;
                }
            }
        }
    }

    mz_zip_reader_end(&zip);
    return result;
}

} // namespace liquidgov::platform
