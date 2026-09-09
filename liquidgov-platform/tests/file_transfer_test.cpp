#include "liquidgov/platform/file_transfer.hpp"

#include "liquidgov/platform/image_normalize.hpp"
#include "miniz.h"
#include "miniz_zip.h"

#include <cassert>
#include <iostream>

using namespace liquidgov::platform;

namespace {

// Builds a real, valid zip in memory using the real writer — same
// lesson as the avatar test fixture fix: generate fixtures with the
// real library, never hand-write the bytes.
std::vector<std::uint8_t> build_zip(
    const std::vector<std::pair<std::string, std::string>>& entries) {
    mz_zip_archive zip{};
    mz_zip_writer_init_heap(&zip, 0, 0);
    for (const auto& [name, content] : entries) {
        mz_zip_writer_add_mem(&zip, name.c_str(), content.data(), content.size(),
                               MZ_DEFAULT_COMPRESSION);
    }
    void* buf = nullptr;
    size_t size = 0;
    mz_zip_writer_finalize_heap_archive(&zip, &buf, &size);
    std::vector<std::uint8_t> out(static_cast<std::uint8_t*>(buf),
                                   static_cast<std::uint8_t*>(buf) + size);
    mz_free(buf);
    mz_zip_writer_end(&zip);
    return out;
}

const char* kCleanContentTypes =
    R"(<?xml version="1.0"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
    R"(<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>)"
    R"(</Types>)";

const char* kMacroContentTypes =
    R"(<?xml version="1.0"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
    R"(<Override PartName="/word/vbaProject.bin" ContentType="application/vnd.ms-office.vbaProject"/>)"
    R"(</Types>)";

std::vector<std::uint8_t> not_a_recognized_type() {
    // Bytes that don't match any allow-listed signature and don't look
    // like plain text either (embedded nulls).
    return {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03, 0x00, 0xFF};
}

class AlwaysClean : public MalwareScanner {
public:
    bool is_clean(const std::vector<std::uint8_t>&) const override { return true; }
};

class AlwaysDirty : public MalwareScanner {
public:
    bool is_clean(const std::vector<std::uint8_t>&) const override { return false; }
};

std::vector<std::uint8_t> solid_png(int w, int h) {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w) * h * 4, 100);
    return encode_png(w, h, pixels);
}

} // namespace

int main() {
    FilesystemBlobStore blobs("/tmp/lgp-filetransfer-test-store");
    AlwaysClean clean;
    SizeCaps caps; // defaults

    // 1. A real, valid image is normalized and stored.
    auto r1 = ingest_file(blobs, clean, caps, solid_png(4, 4));
    assert(r1.error == FileTransferError::None);
    assert(r1.stored_mime_type == "image/png");
    std::cout << "valid image ingests and normalizes\n";

    // 2. A macro-free OOXML document, built as a real zip, passes.
    auto clean_docx = build_zip({
        {"[Content_Types].xml", kCleanContentTypes},
        {"word/document.xml", "<w:document>hello</w:document>"},
    });
    auto r2 = ingest_file(blobs, clean, caps, clean_docx);
    assert(r2.error == FileTransferError::None);
    std::cout << "macro-free OOXML document ingests\n";

    // 3. The SAME shape of document, but with a VBA content-type
    //    declaration — this is the actual macro-detection proof, using
    //    the content-types/relationships check, not a filename grep.
    auto macro_docx = build_zip({
        {"[Content_Types].xml", kMacroContentTypes},
        {"word/vbaProject.bin", "fake macro bytes here"},
    });
    auto r3 = ingest_file(blobs, clean, caps, macro_docx);
    assert(r3.error == FileTransferError::ActiveContentBlocked);
    std::cout << "OOXML with a VBA content-type declaration is rejected\n";

    // 4. An embeddings/ path is rejected independent of the content-type
    //    declarations being otherwise clean.
    auto embedded_object_docx = build_zip({
        {"[Content_Types].xml", kCleanContentTypes},
        {"word/embeddings/oleObject1.bin", "opaque embedded object bytes"},
    });
    auto r4 = ingest_file(blobs, clean, caps, embedded_object_docx);
    assert(r4.error == FileTransferError::ActiveContentBlocked);
    std::cout << "an embeddings/ path is rejected regardless of content-type cleanliness\n";

    // 5. The sharpest proof: a member renamed to something innocuous
    //    (not vbaProject.bin, not mentioning macros anywhere) but whose
    //    own bytes start with the real OLE compound-file signature.
    //    A filename-only check would miss this entirely; the
    //    content-based signature check must not.
    std::string ole_signature_bytes;
    ole_signature_bytes.push_back(static_cast<char>(0xD0));
    ole_signature_bytes.push_back(static_cast<char>(0xCF));
    ole_signature_bytes.push_back(static_cast<char>(0x11));
    ole_signature_bytes.push_back(static_cast<char>(0xE0));
    ole_signature_bytes.push_back(static_cast<char>(0xA1));
    ole_signature_bytes.push_back(static_cast<char>(0xB1));
    ole_signature_bytes.push_back(static_cast<char>(0x1A));
    ole_signature_bytes.push_back(static_cast<char>(0xE1));
    ole_signature_bytes += "trailing padding so the entry has real content";

    auto renamed_ole_docx = build_zip({
        {"[Content_Types].xml", kCleanContentTypes}, // declares nothing dangerous
        {"word/media/totally_a_normal_picture.dat", ole_signature_bytes}, // innocuous name
    });
    auto r5 = ingest_file(blobs, clean, caps, renamed_ole_docx);
    assert(r5.error == FileTransferError::ActiveContentBlocked);
    std::cout << "a renamed OLE-signature member is caught by content, not by filename\n";

    // 6. Bytes that don't match any allow-listed type are rejected.
    auto r6 = ingest_file(blobs, clean, caps, not_a_recognized_type());
    assert(r6.error == FileTransferError::TypeNotAllowed);
    std::cout << "unrecognized type is rejected\n";

    // 7. Oversized input is rejected before any parsing.
    std::vector<std::uint8_t> huge(caps.protocol_max_bytes + 1, 0x41);
    auto r7 = ingest_file(blobs, clean, caps, huge);
    assert(r7.error == FileTransferError::TooLarge);
    std::cout << "oversized input rejected on length alone\n";

    // 8. A structurally valid, allow-listed file that fails the malware
    //    scan is still rejected — the scan runs regardless of type.
    AlwaysDirty dirty;
    auto r8 = ingest_file(blobs, dirty, caps, solid_png(4, 4));
    assert(r8.error == FileTransferError::FailedScan);
    std::cout << "an otherwise-valid file failing the malware scan is rejected\n";

    std::cout << "\nall file-transfer tests passed\n";
    return 0;
}
