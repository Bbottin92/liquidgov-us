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

} // namespace

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
    if (starts_with(bytes, {'P', 'K', 0x03, 0x04})) {
        // Prefix match alone is not proof — verify with a real zip
        // reader that actually parses the central directory (located at
        // the end of the file, not the start). A prefix-only check is
        // exactly what a polyglot is built to pass while failing this
        // real structural parse.
        mz_zip_archive zip{};
        if (mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0)) {
            mz_zip_reader_end(&zip);
            return DetectedType::OoxmlZipContainer;
        }
        return DetectedType::Unknown; // looked like a zip, did not actually parse as one
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
