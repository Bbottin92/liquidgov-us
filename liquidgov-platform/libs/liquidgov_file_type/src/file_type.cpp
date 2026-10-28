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
    // .bin, not none: with no forced extension, a Linux program could download as "invoice.pdf" (review 2026-10-27)
    {ProgramKind::LinuxElf, "elf", "Linux program (.bin)", ".bin", "application/x-elf"},
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

namespace {

// A program's own leading magic (not an embedded one), or Unknown. Checked
// before every other type (review 2026-10-27): an .exe whose DOS header has
// "ftyp" at byte 4 is still an .exe, and Windows runs it as one.
DetectedType program_magic(const std::vector<std::uint8_t>& bytes) {
    if (starts_with(bytes, {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C})) return DetectedType::SevenZip;
    if (starts_with(bytes, {'R', 'a', 'r', '!', 0x1A, 0x07})) return DetectedType::Rar;
    if (starts_with(bytes, {0x7F, 'E', 'L', 'F'})) return DetectedType::LinuxElf;
    if (is_pe_executable(bytes)) return DetectedType::WindowsExe;
    if (is_msi(bytes)) return DetectedType::WindowsMsi;
    return DetectedType::Unknown;
}

// An OOXML candidate: a zip with [Content_Types].xml. That alone proves
// little (review 2026-10-27: an empty one plus payload.exe passed as a Word
// file), so inspect_ooxml_container also refuses any entry that is a
// program or archive by content, or a script / executable by name.
// Requiring a main part (word/document.xml ...) was considered: an empty
// one is as easy to add, and the committed fixtures don't have one.
bool is_ooxml_package(mz_zip_archive& zip) {
    return zip_has(zip, "[Content_Types].xml");
}

// Entry names a document never needs and Windows runs or opens as code.
bool runnable_name(const std::string& lower_name) {
    static const char* kRunnable[] = {".exe", ".dll", ".com", ".scr", ".msi", ".msp", ".bat", ".cmd", ".ps1", ".psm1",
                                      ".vbs", ".vbe", ".js", ".jse", ".wsf", ".wsh", ".hta", ".jar", ".lnk", ".reg",
                                      ".cpl", ".pif", ".apk", ".sh", ".zip", ".7z", ".rar",
                                      // second review 2026-10-27: disk images, cabinets, app packages, Office
                                      // add-ins, shortcuts and the archive formats whose content the first 4 KB
                                      // doesn't give away
                                      ".iso", ".img", ".vhd", ".vhdx", ".cab", ".msix", ".appx", ".chm", ".xll",
                                      ".url", ".sct", ".wsc", ".scf", ".gz", ".tar", ".tgz", ".xz", ".bz2"};
    for (const char* ext : kRunnable) {
        const std::size_t n = std::strlen(ext);
        if (lower_name.size() >= n && lower_name.compare(lower_name.size() - n, n, ext) == 0) return true;
    }
    return false;
}

} // namespace

DetectedType embedded_archive(const std::vector<std::uint8_t>& bytes) {
    // Zip readers find the central directory from the end, so "%PDF" plus
    // a zip opens as a zip; RAR and 7-Zip readers look for their signature
    // anywhere in front.
    mz_zip_archive zip{};
    if (mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0)) {
        mz_zip_reader_end(&zip);
        return DetectedType::Zip;
    }
    // miniz refuses a zip with data in front (its offsets are off by the
    // prefix), but 7-Zip, unzip and Python open one with data in front or
    // behind. So look anywhere for an end-of-central-directory record
    // ("PK\5\6") whose central directory -- size at +12 -- ends right
    // where the record starts and begins with a directory header
    // ("PK\1\2"). That holds whatever is in front or behind, and two
    // signatures at an exact distance don't happen by chance (second
    // review 2026-10-27: a trailing byte defeated "the comment reaches the
    // end of the file").
    static const std::uint8_t kEndRecord[4] = {'P', 'K', 0x05, 0x06};
    for (auto it = std::search(bytes.begin(), bytes.end(), kEndRecord, kEndRecord + 4); it != bytes.end();
         it = std::search(it + 1, bytes.end(), kEndRecord, kEndRecord + 4)) {
        const std::size_t p = static_cast<std::size_t>(it - bytes.begin());
        if (p + 22 > bytes.size()) break;
        const std::size_t dir_size = read_u32_le(bytes, p + 12);
        if (dir_size >= 46 && dir_size <= p) { // 46: the smallest central directory header
            const std::size_t dir = p - dir_size;
            if (bytes[dir] == 'P' && bytes[dir + 1] == 'K' && bytes[dir + 2] == 0x01 && bytes[dir + 3] == 0x02)
                return DetectedType::Zip;
        }
    }
    static const std::uint8_t kSevenZip[6] = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};
    static const std::uint8_t kRar[6] = {'R', 'a', 'r', '!', 0x1A, 0x07};
    if (std::search(bytes.begin(), bytes.end(), kSevenZip, kSevenZip + 6) != bytes.end()) return DetectedType::SevenZip;
    if (std::search(bytes.begin(), bytes.end(), kRar, kRar + 6) != bytes.end()) return DetectedType::Rar;
    return DetectedType::Unknown;
}

DetectedType detect_type(const std::vector<std::uint8_t>& bytes) {
    if (DetectedType program = program_magic(bytes); program != DetectedType::Unknown) return program;
    if (starts_with(bytes, {'P', 'K', 0x03, 0x04})) {
        // Prefix match alone is not proof — verify with a real zip
        // reader that actually parses the central directory (located at
        // the end of the file, not the start). A prefix-only check is
        // exactly what a polyglot is built to pass while failing this
        // real structural parse.
        mz_zip_archive zip{};
        if (mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0)) {
            DetectedType type = DetectedType::Zip;
            if (is_ooxml_package(zip)) type = DetectedType::OoxmlZipContainer;
            else if (zip_has(zip, "AndroidManifest.xml") && zip_has(zip, "classes.dex")) type = DetectedType::AndroidApk;
            mz_zip_reader_end(&zip);
            return type;
        }
        return DetectedType::Unknown; // looked like a zip, did not actually parse as one
    }
    if (starts_with(bytes, {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A})) return DetectedType::Png;
    if (starts_with(bytes, {0xFF, 0xD8, 0xFF})) return DetectedType::Jpeg;
    if (starts_with(bytes, {'G', 'I', 'F', '8'})) return DetectedType::Gif;
    // ISO base media (MP4/MOV/M4V): a 4-byte box-size field then the
    // literal box type "ftyp" at offset 4 -- true for every brand
    // (isom/mp42/qt  /avc1/...), so the brand itself isn't checked, only
    // that it is four printable characters and the first box has a sane
    // size (8 bytes to 4 KB).
    if (bytes.size() >= 12 && bytes[4] == 'f' && bytes[5] == 't' && bytes[6] == 'y' && bytes[7] == 'p') {
        const std::uint32_t box = (static_cast<std::uint32_t>(bytes[0]) << 24) | (static_cast<std::uint32_t>(bytes[1]) << 16) |
                                  (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
        bool brand_ok = true;
        for (std::size_t i = 8; i < 12; ++i) brand_ok = brand_ok && bytes[i] >= 0x20 && bytes[i] <= 0x7E;
        return box >= 8 && box <= 4096 && box <= bytes.size() && brand_ok ? DetectedType::Mp4 : DetectedType::Unknown;
    }
    if (starts_with(bytes, {0x1A, 0x45, 0xDF, 0xA3})) return DetectedType::WebM;
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
        if (runnable_name(lower_name)) {
            result.has_macro_content = true;
            result.rejection_reason = "runnable file at " + entry_name;
            mz_zip_reader_end(&zip);
            return result;
        }
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
        // signature (where a renamed vbaProject.bin or oleObjectN.bin
        // actually lives), or with a program or archive (review
        // 2026-10-27: a document must not carry an .exe or a zip)? Only
        // the first 4 KB of each entry is decompressed, whatever its
        // size, so a large entry is checked too and a zip bomb costs 4 KB.
        if (stat.m_uncomp_size >= 2) {
            std::vector<std::uint8_t> head(static_cast<std::size_t>(std::min<mz_uint64>(stat.m_uncomp_size, 4096)));
            std::size_t got = 0;
            if (mz_zip_reader_extract_iter_state* it = mz_zip_reader_extract_iter_new(&zip, i, 0)) {
                got = mz_zip_reader_extract_iter_read(it, head.data(), head.size());
                mz_zip_reader_extract_iter_free(it);
            }
            const bool unreadable = got < head.size(); // encrypted, or a method miniz can't read (second review)
            head.resize(got);
            static const std::uint8_t kOleSignature[] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
            const char* why = nullptr;
            if (unreadable) why = "unreadable entry at ";
            else if (head.size() >= 8 && std::memcmp(head.data(), kOleSignature, 8) == 0) why = "OLE compound file at ";
            else if (program_magic(head) != DetectedType::Unknown || starts_with(head, {'M', 'Z'})) why = "program at ";
            else if (starts_with(head, {'P', 'K', 0x03, 0x04})) why = "archive at ";
            if (why) {
                result.has_macro_content = true;
                result.rejection_reason = why + entry_name;
                mz_zip_reader_end(&zip);
                return result;
            }
        }
    }

    mz_zip_reader_end(&zip);
    return result;
}

} // namespace liquidgov::platform
