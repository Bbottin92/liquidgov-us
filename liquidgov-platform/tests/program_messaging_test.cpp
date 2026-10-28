// Programs in mail (2026-10-27): archives, installers and executables are
// detected by content, refused for everyone without a ProgramPolicy, taken
// only in the kinds and size the policy allows, and the operator's grants
// and the programs' download names survive a reload. Built so it also
// checks in a Release build (asserts kept on).
#undef NDEBUG
#include "liquidgov/platform/auth.hpp"
#include "liquidgov/platform/file_store.hpp"
#include "liquidgov/platform/file_transfer.hpp"
#include "liquidgov/platform/file_type.hpp"
#include "miniz.h"
#include "miniz_zip.h"

#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <unistd.h>

using namespace liquidgov::platform;

namespace {

// Real zips from the real writer (same rule as file_transfer_test).
std::vector<std::uint8_t> build_zip(const std::vector<std::pair<std::string, std::string>>& entries) {
    mz_zip_archive zip{};
    mz_zip_writer_init_heap(&zip, 0, 0);
    for (const auto& [name, content] : entries)
        mz_zip_writer_add_mem(&zip, name.c_str(), content.data(), content.size(), MZ_DEFAULT_COMPRESSION);
    void* buf = nullptr;
    size_t size = 0;
    mz_zip_writer_finalize_heap_archive(&zip, &buf, &size);
    std::vector<std::uint8_t> out(static_cast<std::uint8_t*>(buf), static_cast<std::uint8_t*>(buf) + size);
    mz_free(buf);
    mz_zip_writer_end(&zip);
    return out;
}

// The smallest file a PE loader's first check accepts: a DOS header whose
// e_lfanew (0x3C) points at "PE\0\0". `pdf_in_header` puts "%PDF" inside
// the first KB, which the PDF search would otherwise match.
std::vector<std::uint8_t> pe_file(bool pdf_in_header = false) {
    std::vector<std::uint8_t> b(0x200, 0);
    b[0] = 'M'; b[1] = 'Z';
    b[0x3C] = 0x80;
    b[0x80] = 'P'; b[0x81] = 'E';
    if (pdf_in_header) std::memcpy(b.data() + 0x100, "%PDF-1.4", 8);
    return b;
}

// An OLE compound file: a 512-byte header (signature, sector shift 9 at
// 0x1E, first directory sector 0 at 0x30), then the directory sector whose
// first entry (the root) carries `clsid` at offset 0x50.
std::vector<std::uint8_t> cfb_file(const std::uint8_t clsid[16]) {
    std::vector<std::uint8_t> b(1024, 0);
    const std::uint8_t sig[8] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
    std::memcpy(b.data(), sig, 8);
    b[0x1E] = 9;
    std::memcpy(b.data() + 512 + 0x50, clsid, 16);
    return b;
}
const std::uint8_t kMsiClsid[16] = {0x84, 0x10, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00,
                                    0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};
const std::uint8_t kWordClsid[16] = {0x06, 0x09, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};

std::vector<std::uint8_t> with_magic(std::initializer_list<int> magic) {
    std::vector<std::uint8_t> b;
    for (int m : magic) b.push_back(static_cast<std::uint8_t>(m));
    b.resize(64, 0);
    return b;
}

class AlwaysClean : public MalwareScanner {
public:
    bool is_clean(const std::vector<std::uint8_t>&) const override { return true; }
};
class AlwaysDirty : public MalwareScanner {
public:
    bool is_clean(const std::vector<std::uint8_t>&) const override { return false; }
};
class NeverBreached : public BreachChecker {
public:
    bool is_breached(const std::string&) const override { return false; }
};

std::string tmp_path() {
    char buf[] = "/tmp/lgp-programs-XXXXXX";
    int fd = mkstemp(buf);
    assert(fd >= 0);
    close(fd);
    unlink(buf);
    return std::string(buf) + ".lgp";
}

std::uint32_t bit(ProgramKind k) { return static_cast<std::uint32_t>(k); }

} // namespace

int main() {
    // --- detection ---
    const auto plain_zip = build_zip({{"Jammo/Jammo.exe", "not really a program"}, {"Jammo/readme.txt", "hi"}});
    assert(detect_type(plain_zip) == DetectedType::Zip);
    assert(detect_type(build_zip({{"[Content_Types].xml", "<Types/>"}, {"word/document.xml", "<w/>"}})) ==
           DetectedType::OoxmlZipContainer);
    assert(detect_type(build_zip({{"AndroidManifest.xml", "x"}, {"classes.dex", "dex"}})) == DetectedType::AndroidApk);
    assert(detect_type(pe_file()) == DetectedType::WindowsExe);
    assert(detect_type(pe_file(true)) == DetectedType::WindowsExe); // not a PDF
    {
        auto mz_only = pe_file();
        mz_only[0x80] = 0;                                           // "MZ" without a PE header
        assert(detect_type(mz_only) == DetectedType::Unknown);
    }
    assert(detect_type(cfb_file(kMsiClsid)) == DetectedType::WindowsMsi);
    assert(detect_type(cfb_file(kWordClsid)) == DetectedType::Unknown); // legacy .doc stays refused
    assert(detect_type(with_magic({'7', 'z', 0xBC, 0xAF, 0x27, 0x1C})) == DetectedType::SevenZip);
    assert(detect_type(with_magic({'R', 'a', 'r', '!', 0x1A, 0x07, 0x00})) == DetectedType::Rar);
    assert(detect_type(with_magic({0x7F, 'E', 'L', 'F'})) == DetectedType::LinuxElf);
    std::cout << "program kinds detected by content (zip, apk, exe, msi, 7z, rar, elf; .doc and bare MZ refused)\n";

    std::uint32_t all = 0;
    for (const auto& k : program_kinds()) {
        all |= bit(k.kind);
        assert(program_kind_by_mime(k.mime) == &k);
    }
    assert(all == kAllProgramKinds);
    assert(program_kind_of(DetectedType::Png) == nullptr);
    assert(program_kind_of(DetectedType::OoxmlZipContainer) == nullptr);
    std::cout << "every kind has a bit, a slug and a mime\n";

    // --- ingest ---
    FilesystemBlobStore blobs("/tmp/lgp-programs-test-blobs");
    AlwaysClean clean;
    SizeCaps caps;

    // A plain zip used to pass as a Word file; now it is a program and
    // nobody without a policy may send it.
    auto r1 = ingest_file(blobs, clean, caps, plain_zip);
    assert(r1.error == FileTransferError::ProgramNotAllowed);
    auto r2 = ingest_file(blobs, clean, caps, pe_file());
    assert(r2.error == FileTransferError::ProgramNotAllowed);
    std::cout << "no policy: archives and programs refused\n";

    ProgramPolicy zip_only{bit(ProgramKind::Zip), 1024 * 1024};
    auto r3 = ingest_file(blobs, clean, caps, plain_zip, &zip_only);
    assert(r3.error == FileTransferError::None);
    assert(r3.stored_mime_type == "application/zip");
    assert(r3.blob_id && blobs.get(*r3.blob_id) == plain_zip); // stored as sent
    auto r4 = ingest_file(blobs, clean, caps, pe_file(), &zip_only);
    assert(r4.error == FileTransferError::ProgramNotAllowed);
    std::cout << "a policy allows exactly its kinds\n";

    ProgramPolicy tiny{bit(ProgramKind::Zip), plain_zip.size() - 1};
    assert(ingest_file(blobs, clean, caps, plain_zip, &tiny).error == FileTransferError::TooLarge);
    ProgramPolicy exact{bit(ProgramKind::Zip), plain_zip.size()};
    assert(ingest_file(blobs, clean, caps, plain_zip, &exact).error == FileTransferError::None);
    std::cout << "a policy's size limit is inclusive\n";

    AlwaysDirty dirty;
    ProgramPolicy everything{kAllProgramKinds, caps.protocol_max_bytes};
    assert(ingest_file(blobs, dirty, caps, plain_zip, &everything).error == FileTransferError::FailedScan);
    std::cout << "the malware scan still runs on programs\n";

    // The fresh review's bypasses (2026-10-27): each would have let anyone
    // send a program that a zip / RAR reader or Windows still opens.
    {
        // a zip with an (empty) [Content_Types].xml and an .exe passed as a Word file
        const auto fake_doc = build_zip({{"[Content_Types].xml", ""}, {"payload.exe", std::string(pe_file().begin(), pe_file().end())}});
        assert(ingest_file(blobs, clean, caps, fake_doc).error == FileTransferError::ActiveContentBlocked);
        const auto doc_with_script = build_zip({{"[Content_Types].xml", "<Types/>"}, {"word/document.xml", "<w/>"},
                                               {"word/media/run.js", "WScript.Echo(1)"}});
        assert(ingest_file(blobs, clean, caps, doc_with_script).error == FileTransferError::ActiveContentBlocked);
        const auto doc_with_zip = build_zip({{"[Content_Types].xml", "<Types/>"}, {"word/document.xml", "<w/>"},
                                            {"word/media/image1.png", std::string(plain_zip.begin(), plain_zip.end())}});
        assert(ingest_file(blobs, clean, caps, doc_with_zip).error == FileTransferError::ActiveContentBlocked);
        // a large entry is checked too (the old OLE check skipped entries of 20 MB or more)
        std::string big_mz(21 * 1024 * 1024, 'x');
        big_mz[0] = 'M'; big_mz[1] = 'Z';
        const auto doc_with_big_exe = build_zip({{"[Content_Types].xml", "<Types/>"}, {"word/media/big.bin", big_mz}});
        assert(ingest_file(blobs, clean, caps, doc_with_big_exe).error == FileTransferError::ActiveContentBlocked);

        // an .exe with "ftyp" at bytes 4..7 passed as an MP4
        auto exe_as_mp4 = pe_file();
        std::memcpy(exe_as_mp4.data() + 4, "ftyp", 4);
        assert(detect_type(exe_as_mp4) == DetectedType::WindowsExe);
        assert(ingest_file(blobs, clean, caps, exe_as_mp4).error == FileTransferError::ProgramNotAllowed);

        // "%PDF" in front of a zip passed as a PDF and still opened as a zip
        std::vector<std::uint8_t> pdf_zip = {'%', 'P', 'D', 'F', '-', '1', '.', '4', '\n'};
        pdf_zip.insert(pdf_zip.end(), plain_zip.begin(), plain_zip.end());
        assert(ingest_file(blobs, clean, caps, pdf_zip).error == FileTransferError::ProgramNotAllowed);
        // ...even with a byte after the zip (second review: the end-of-file check missed it)
        auto pdf_zip_tail = pdf_zip;
        pdf_zip_tail.push_back(0);
        assert(ingest_file(blobs, clean, caps, pdf_zip_tail).error == FileTransferError::ProgramNotAllowed);
        // a disk image inside a document is refused by name
        const auto doc_with_iso = build_zip({{"[Content_Types].xml", "<Types/>"}, {"word/document.xml", "<w/>"},
                                            {"word/media/disk.iso", "CD001 pretend"}});
        assert(ingest_file(blobs, clean, caps, doc_with_iso).error == FileTransferError::ActiveContentBlocked);
        // and RAR / 7-Zip signatures behind a PDF
        std::vector<std::uint8_t> pdf_rar = {'%', 'P', 'D', 'F', '-', '1', '.', '4', '\n', 'R', 'a', 'r', '!', 0x1A, 0x07, 0x00};
        assert(ingest_file(blobs, clean, caps, pdf_rar).error == FileTransferError::ProgramNotAllowed);

        // a real-looking MP4 header with a zip after it
        std::vector<std::uint8_t> mp4_zip = {0, 0, 0, 0x18, 'f', 't', 'y', 'p', 'i', 's', 'o', 'm', 0, 0, 2, 0,
                                             'i', 's', 'o', 'm', 'm', 'p', '4', '1'};
        assert(detect_type(mp4_zip) == DetectedType::Mp4);
        mp4_zip.insert(mp4_zip.end(), plain_zip.begin(), plain_zip.end());
        assert(ingest_file(blobs, clean, caps, mp4_zip).error == FileTransferError::ProgramNotAllowed);
        // an "ftyp" box with a silly size is not an MP4
        std::vector<std::uint8_t> bad_box = {0x7F, 0, 0, 0, 'f', 't', 'y', 'p', 'i', 's', 'o', 'm'};
        assert(detect_type(bad_box) != DetectedType::Mp4);

        // still fine: a plain PDF and a plain MP4 header, from anyone
        std::vector<std::uint8_t> pdf = {'%', 'P', 'D', 'F', '-', '1', '.', '4', '\n', '%', '%', 'E', 'O', 'F', '\n'};
        assert(ingest_file(blobs, clean, caps, pdf).error == FileTransferError::None);
        std::vector<std::uint8_t> mp4 = {0, 0, 0, 0x18, 'f', 't', 'y', 'p', 'i', 's', 'o', 'm', 0, 0, 2, 0,
                                         'i', 's', 'o', 'm', 'm', 'p', '4', '1'};
        assert(ingest_file(blobs, clean, caps, mp4).error == FileTransferError::None);
        // and a granted sender's program inside a PDF is stored as the archive it is
        auto r = ingest_file(blobs, clean, caps, pdf_zip, &zip_only);
        assert(r.error == FileTransferError::None && r.stored_mime_type == "application/zip");
    }
    std::cout << "review bypasses closed: fake Office zips, scripts / programs / archives inside documents, "
                 "exe-as-MP4, archives behind a PDF or video\n";

    // A policy never lifts the non-program caps or the protocol ceiling.
    std::vector<std::uint8_t> huge(caps.protocol_max_bytes + 1, 0x41);
    assert(ingest_file(blobs, clean, caps, huge, &everything).error == FileTransferError::TooLarge);
    std::cout << "the protocol ceiling holds for program senders too\n";

    // --- store ---
    NeverBreached breach;
    const std::string path = tmp_path();
    {
        FileStore store(path);
        assert(create_account(store, breach, "friend", "correct-horse-battery-staple", "+15551212").error ==
               AuthError::None);
        assert(!store.program_grant("friend"));
        ProgramGrant g;
        g.username = "friend";
        g.kinds = bit(ProgramKind::Zip) | bit(ProgramKind::WindowsExe);
        g.max_bytes = 80ull * 1024 * 1024;
        g.granted_by = "Bbottin";
        g.granted_at = 1793534400;
        store.set_program_grant(g);

        // the same bytes sent twice under two names keep both (names are per message)
        assert(create_account(store, breach, "pal", "correct-horse-battery-staple", "+15551213").error == AuthError::None);
        store.add_mail("friend", "pal", "", std::string("abc123"), std::string("application/zip"));
        store.add_mail("pal", "friend", "", std::string("abc123"), std::string("application/zip"));
        const auto thread = store.mail_with("friend", "pal");
        assert(thread.size() == 2);
        for (const auto& m : thread)
            store.set_attachment_name(attachment_name_key(m), m.from == "friend" ? "Jammo 1.0.zip" : "same bytes.zip");
        store.set_attachment_name("d|ghost|nobody|1|abc123", "orphan.zip"); // no such message
    }
    {
        FileStore store(path); // reload from disk
        auto g = store.program_grant("friend");
        assert(g && g->kinds == (bit(ProgramKind::Zip) | bit(ProgramKind::WindowsExe)));
        assert(g->max_bytes == 80ull * 1024 * 1024 && g->granted_by == "Bbottin" && g->granted_at == 1793534400);
        const auto thread = store.mail_with("friend", "pal");
        for (const auto& m : thread)
            assert(store.attachment_name(attachment_name_key(m)) ==
                   std::optional<std::string>(m.from == "friend" ? "Jammo 1.0.zip" : "same bytes.zip"));
        assert(!store.attachment_name("d|ghost|nobody|1|abc123")); // pruned at load
        // deleting an account drops the names of the mail it sent or received
        auto pal = store.find_by_username("pal");
        assert(pal && store.delete_account(pal->id));
        for (const auto& m : thread) assert(!store.attachment_name(attachment_name_key(m)));

        ProgramGrant none;
        none.username = "friend";
        store.set_program_grant(none); // no kinds = revoke
        assert(!store.program_grant("friend"));

        ProgramGrant again;
        again.username = "friend";
        again.kinds = bit(ProgramKind::Zip);
        again.max_bytes = 1;
        store.set_program_grant(again);
        auto acc = store.find_by_username("friend");
        assert(acc && store.delete_account(acc->id));
        assert(!store.program_grant("friend"));
    }
    {
        FileStore store(path);
        assert(!store.program_grant("friend")); // deleting the account removed it on disk too
    }
    unlink(path.c_str());
    std::cout << "grants and download names persist; revoke and account deletion remove the grant\n";

    // Every record type the loader reads has its own tag. The grant was
    // first written as "pg", which already meant "post in an extra
    // community": the reload threw, and on the live file it would have
    // stopped the site from starting (lessons 2026-10-27).
    {
        std::ifstream src(std::string(LG_SOURCE_DIR) + "/libs/liquidgov_file_store/src/file_store.cpp");
        assert(src.good());
        const std::string text((std::istreambuf_iterator<char>(src)), std::istreambuf_iterator<char>());
        std::set<std::string> tags;
        std::size_t count = 0;
        const std::string marker = "tag == \"";
        for (std::size_t at = text.find(marker); at != std::string::npos; at = text.find(marker, at + 1)) {
            const std::size_t start = at + marker.size();
            const std::string tag = text.substr(start, text.find('"', start) - start);
            ++count;
            if (!tags.insert(tag).second) std::cerr << "duplicate store tag: " << tag << "\n";
        }
        assert(count > 20 && tags.size() == count);
    }
    std::cout << "every store record tag is unique\n";

    std::cout << "\nall program-messaging tests passed\n";
    return 0;
}
