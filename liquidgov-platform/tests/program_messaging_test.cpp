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
        store.set_attachment_name("abc123", "Bbottin", "Jammo 1.0.zip");
        store.set_attachment_name("abc123", "friend", "same bytes.zip"); // same blob, another sender
    }
    {
        FileStore store(path); // reload from disk
        auto g = store.program_grant("friend");
        assert(g && g->kinds == (bit(ProgramKind::Zip) | bit(ProgramKind::WindowsExe)));
        assert(g->max_bytes == 80ull * 1024 * 1024 && g->granted_by == "Bbottin" && g->granted_at == 1793534400);
        assert(store.attachment_name("abc123", "Bbottin") == std::optional<std::string>("Jammo 1.0.zip"));
        assert(store.attachment_name("abc123", "friend") == std::optional<std::string>("same bytes.zip"));
        assert(!store.attachment_name("abc123", "nobody"));

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
